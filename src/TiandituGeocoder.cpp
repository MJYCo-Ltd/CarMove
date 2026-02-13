#include "TiandituGeocoder.h"
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDir>
#include <QStringConverter>

TiandituGeocoder::TiandituGeocoder(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    loadAdminCodeCsv();
}

TiandituGeocoder::~TiandituGeocoder()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void TiandituGeocoder::searchInAdminRegion(const QString &keyWord, const QString &specifyAdminCode)
{
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
            emit geocodeFailed(tr("未找到行政区「%1」对应的国标码，请使用 AdminCode.csv 中的名称或 9 位国标码").arg(code));
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
    QUrl url("http://api.tianditu.gov.cn/v2/search");
    QUrlQuery query;
    query.addQueryItem("postStr", QString::fromUtf8(postStr));
    query.addQueryItem("type", "query");
    query.addQueryItem("tk", defaultKey);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    setBusy(true);
    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &TiandituGeocoder::onReplyFinished);
}

QString TiandituGeocoder::adminCodeForName(const QString &adminName) const
{
    return m_adminNameToCode.value(adminName.trimmed());
}

QStringList TiandituGeocoder::adminRegionNames() const
{
    return m_adminNames;
}

void TiandituGeocoder::loadAdminCodeCsv()
{
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

void TiandituGeocoder::onReplyFinished()
{
    setBusy(false);
    if (!m_reply) return;

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit geocodeFailed(reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    double lat = 0, lon = 0;
    QString name, address;
    if (!parseAdminSearchReply(data, lat, lon, name, address)) {
        emit geocodeFailed(tr("未找到该地点或解析失败"));
        return;
    }
    emit geocodeSucceeded(lat, lon, name, address);
}

void TiandituGeocoder::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

bool TiandituGeocoder::parseLonLat(const QString &lonlat, double &outLat, double &outLon)
{
    if (lonlat.isEmpty()) return false;
    QStringList parts = lonlat.split(',');
    if (parts.size() < 2) return false;
    bool ok1 = false, ok2 = false;
    double lon = parts[0].trimmed().toDouble(&ok1);
    double lat = parts[1].trimmed().toDouble(&ok2);
    if (!ok1 || !ok2) return false;
    outLon = lon;
    outLat = lat;
    return true;
}

bool TiandituGeocoder::parseAdminSearchReply(const QByteArray &json, double &outLat, double &outLon,
                                             QString &outName, QString &outAddress)
{
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    if (root["infocode"].toInt(0) != 1000)
        return false;

    int resultType = root["resultType"].toInt(0);
    // 1 = 普通 POI（pois），3 = 行政区（area）
    if (resultType == 1) {
        QJsonArray pois = root["pois"].toArray();
        if (pois.isEmpty()) return false;
        QJsonObject first = pois.at(0).toObject();
        QString lonlat = first["lonlat"].toString();
        if (!parseLonLat(lonlat, outLat, outLon)) return false;
        outName = first["name"].toString();
        outAddress = first["address"].toString();
        return true;
    }
    if (resultType == 3) {
        QJsonArray area = root["area"].toArray();
        if (area.isEmpty()) return false;
        QJsonObject first = area.at(0).toObject();
        QString lonlat = first["lonlat"].toString();
        if (!parseLonLat(lonlat, outLat, outLon)) return false;
        outName = first["name"].toString();
        outAddress = QString();
        return true;
    }
    return false;
}
