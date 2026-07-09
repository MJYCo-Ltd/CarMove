#include "Map/TianDiTu/TiandituGeocoder.h"
#include "Core/ConfigManager.h"
#include <cmath>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringConverter>
#include <QTextStream>
#include <QUrlQuery>

TiandituGeocoder::TiandituGeocoder(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this)) {
    loadAdminCodeCsv();
}

TiandituGeocoder::~TiandituGeocoder() {
    if (m_reply) {
        m_reply->abort();
    }
}

void TiandituGeocoder::searchInAdminRegion(const QString &keyWord,
                                           const QString &specifyAdminCode) {
    QString kw = keyWord.trimmed();
    QString code = specifyAdminCode.trimmed();
    if (kw.isEmpty()) {
        emit geocodeFailed(tr("请输入搜索关键字"));
        return;
    }
    if (code.isEmpty()) {
        emit geocodeFailed(tr("请指定行政区（国标码或名称）"));
        return;
    }

    // 若传入的是行政区名称，查国标码
    if (code.length() != 9 || !code.at(0).isDigit()) {
        QString resolved = m_adminNameToCode.value(code);
        if (resolved.isEmpty())
            resolved = m_adminNameToCode.value(code + "市");
        if (resolved.isEmpty())
            resolved = m_adminNameToCode.value(code + "省");
        if (resolved.isEmpty())
            resolved = m_adminNameToCode.value(code + "自治区");
        if (!resolved.isEmpty())
            code = resolved;
        else {
            emit geocodeFailed(tr("未找到行政区「%1」对应的国标码，请使用 "
                                  "AdminCode.csv 中的名称或 9 位国标码")
                                   .arg(code));
            return;
        }
    }

    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }

    // 1.1 行政区划区域搜索：queryType=12，specify 为 9 位国标码
    QJsonObject postObj;
    postObj["keyWord"] = kw;
    postObj["queryType"] = 12;
    postObj["specify"] = code;
    postObj["start"] = 0;
    postObj["count"] = 10;

    QByteArray postStr = QJsonDocument(postObj).toJson(QJsonDocument::Compact);
    QUrl url("https://api.tianditu.gov.cn/v2/search");
    QUrlQuery query;
    query.addQueryItem("postStr", QString::fromUtf8(postStr));
    query.addQueryItem("type", "query");
    query.addQueryItem("tk", ConfigManager::GetInstance()->tiandituKey());
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_requestKind = GeocoderRequestKind::AdminRegionSearch;
    setBusy(true);
    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::finished, this,
            &TiandituGeocoder::onReplyFinished);
}

void TiandituGeocoder::reverseGeocode(double longitude, double latitude) {
    const QString tk = ConfigManager::GetInstance()->tiandituKey();
    if (tk.isEmpty()) {
        emit reverseGeocodeFailed(tr("未配置天地图密钥（tk）"));
        return;
    }
    if (!std::isfinite(longitude) || !std::isfinite(latitude)) {
        emit reverseGeocodeFailed(tr("无效的经纬度"));
        return;
    }
    m_reverseRequestLon = longitude;
    m_reverseRequestLat = latitude;

    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }

    QJsonObject postObj;
    postObj["lon"] = longitude;
    postObj["lat"] = latitude;
    postObj["ver"] = 1;

    const QByteArray postStr = QJsonDocument(postObj).toJson(QJsonDocument::Compact);
    QUrl url(QStringLiteral("https://api.tianditu.gov.cn/geocoder"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("postStr"), QString::fromUtf8(postStr));
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("geocode"));
    query.addQueryItem(QStringLiteral("tk"), tk);
    url.setQuery(query);

    QNetworkRequest req(url);
    m_requestKind = GeocoderRequestKind::ReverseGeocode;
    setBusy(true);
    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &TiandituGeocoder::onReplyFinished);
}

QString TiandituGeocoder::adminCodeForName(const QString &adminName) const {
    return m_adminNameToCode.value(adminName.trimmed());
}

QStringList TiandituGeocoder::adminRegionNames() const { return m_adminNames; }

void TiandituGeocoder::loadAdminCodeCsv() {
    m_adminNameToCode.clear();
    m_adminNames.clear();
    QString path = QCoreApplication::applicationDirPath() + "/AdminCode.csv";
    if (!QFile::exists(path))
        path = ":/AdminCode.csv";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;
        QStringList parts = line.split(',');
        if (parts.size() < 3)
            continue;
        QString name = parts[0].trimmed();
        QString gbCode = parts[2].trimmed();
        if (name.isEmpty() || gbCode.length() != 9)
            continue;
        m_adminNameToCode.insert(name, gbCode);
        m_adminNames.append(name);
    }
}

void TiandituGeocoder::onReplyFinished() {
    setBusy(false);
    if (!m_reply)
        return;

    const GeocoderRequestKind kind = m_requestKind;
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    m_requestKind = GeocoderRequestKind::None;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        const QString err = reply->errorString();
        if (kind == GeocoderRequestKind::ReverseGeocode)
            emit reverseGeocodeFailed(err);
        else
            emit geocodeFailed(err);
        return;
    }

    const QByteArray data = reply->readAll();

    if (kind == GeocoderRequestKind::ReverseGeocode) {
        QString poi;
        QString address;
        double outLat = 0;
        double outLon = 0;
        if (!parseReverseGeocodeReply(data, m_reverseRequestLon, m_reverseRequestLat, poi, address,
                                      outLat, outLon)) {
            emit reverseGeocodeFailed(tr("逆地理编码无结果或解析失败"));
            return;
        }
        emit reverseGeocodeSucceeded(poi, address, outLat, outLon);
        return;
    }

    QVariantList results = parseAllAdminSearchResults(data);
    if (results.isEmpty()) {
        emit geocodeFailed(tr("未找到该地点或解析失败"));
        return;
    }
    emit geocodeResultsReady(results);
}

void TiandituGeocoder::setBusy(bool busy) {
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool TiandituGeocoder::parseLonLat(const QString &lonlat, double &outLat,
                                   double &outLon) {
    if (lonlat.isEmpty())
        return false;
    QStringList parts = lonlat.split(',');
    if (parts.size() < 2)
        return false;
    bool ok1 = false, ok2 = false;
    double lon = parts[0].trimmed().toDouble(&ok1);
    double lat = parts[1].trimmed().toDouble(&ok2);
    if (!ok1 || !ok2)
        return false;
    outLon = lon;
    outLat = lat;
    return true;
}

QVariantList TiandituGeocoder::parseAllAdminSearchResults(const QByteArray &json) {
    QVariantList results;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return results;

    QJsonObject root = doc.object();
    if (root["status"].toObject()["infocode"].toInt() != 1000)
        return results;

    int resultType = root["resultType"].toInt(0);
    if (resultType == 1) {
        const QJsonArray pois = root["pois"].toArray();
        for (const QJsonValue &v : pois) {
            QJsonObject obj = v.toObject();
            double lat = 0, lon = 0;
            if (!parseLonLat(obj["lonlat"].toString(), lat, lon)) continue;
            QVariantMap item;
            item["name"]      = obj["name"].toString();
            item["address"]   = obj["address"].toString();
            item["latitude"]  = lat;
            item["longitude"] = lon;
            results.append(item);
        }
    } else if (resultType == 3) {
        const QJsonArray area = root["area"].toArray();
        for (const QJsonValue &v : area) {
            QJsonObject obj = v.toObject();
            double lat = 0, lon = 0;
            if (!parseLonLat(obj["lonlat"].toString(), lat, lon)) continue;
            QVariantMap item;
            item["name"]      = obj["name"].toString();
            item["address"]   = QString();
            item["latitude"]  = lat;
            item["longitude"] = lon;
            results.append(item);
        }
    }
    return results;
}

bool TiandituGeocoder::parseReverseGeocodeReply(const QByteArray &json, double requestLon,
                                               double requestLat, QString &outPoi, QString &outAddress,
                                               double &outLat, double &outLon) const {
    outPoi.clear();
    outAddress.clear();
    outLat = requestLat;
    outLon = requestLon;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject root = doc.object();
    const auto statusToInt = [](const QJsonValue &v) -> int {
        if (v.isString())
            return v.toString().toInt(nullptr, 10);
        if (v.isDouble())
            return static_cast<int>(v.toDouble());
        return -999;
    };
    if (statusToInt(root.value(QLatin1String("status"))) != 0)
        return false;

    QJsonObject result = root.value(QLatin1String("result")).toObject();
    if (result.isEmpty()) {
        const QString rs = root.value(QLatin1String("result")).toString();
        if (!rs.isEmpty()) {
            const QJsonDocument rd = QJsonDocument::fromJson(rs.toUtf8(), &err);
            if (err.error == QJsonParseError::NoError && rd.isObject())
                result = rd.object();
        }
    }

    const QJsonObject ac = result.value(QLatin1String("addressComponent")).toObject();
    outPoi = ac.value(QLatin1String("poi")).toString().trimmed();

    outAddress = result.value(QLatin1String("formatted_address")).toString();
    if (outAddress.isEmpty()) {
        QString built;
        const QStringList keys{QStringLiteral("nation"), QStringLiteral("province"), QStringLiteral("city"),
                               QStringLiteral("county"), QStringLiteral("town"), QStringLiteral("road"),
                               QStringLiteral("address")};
        for (const QString &k : keys) {
            const QString p = ac.value(k).toString();
            if (!p.isEmpty())
                built += p;
        }
        if (!outPoi.isEmpty())
            built += outPoi;
        outAddress = built;
    }

    const QJsonObject loc = result.value(QLatin1String("location")).toObject();
    if (loc.contains(QLatin1String("lon")) && loc.contains(QLatin1String("lat"))) {
        outLon = loc.value(QLatin1String("lon")).toDouble();
        outLat = loc.value(QLatin1String("lat")).toDouble();
    }

    return !outAddress.isEmpty() || !outPoi.isEmpty();
}
