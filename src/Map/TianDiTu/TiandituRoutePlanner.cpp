#include "Map/TianDiTu/TiandituRoutePlanner.h"
#include "Core/ConfigManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QXmlStreamReader>

TiandituRoutePlanner::TiandituRoutePlanner(QObject *parent)
    : QObject(parent), m_network(new QNetworkAccessManager(this))
{
}

TiandituRoutePlanner::~TiandituRoutePlanner()
{
    if (m_reply)
        m_reply->abort();
}

void TiandituRoutePlanner::requestRoute(double origLon, double origLat, double destLon,
                                        double destLat, int style, const QString &mid)
{
    QString tk = ConfigManager::GetInstance()->tiandituKey().trimmed();
    if (tk.isEmpty()) {
        emit routeFailed(tr("请先在设置中配置天地图密钥（tk）"));
        return;
    }

    QJsonObject postObj;
    postObj["orig"] =
        QString::number(origLon, 'f', 8) + QLatin1Char(',') + QString::number(origLat, 'f', 8);
    postObj["dest"] =
        QString::number(destLon, 'f', 8) + QLatin1Char(',') + QString::number(destLat, 'f', 8);
    const QString midTrim = mid.trimmed();
    if (!midTrim.isEmpty())
        postObj["mid"] = midTrim;
    const int s = qBound(0, style, 3);
    postObj["style"] = QString::number(s);

    sendDriveRequest(postObj);
}

void TiandituRoutePlanner::sendDriveRequest(const QJsonObject &postObj)
{
    if (m_reply) {
        m_reply->abort();
        m_reply = nullptr;
    }

    const QByteArray postStr = QJsonDocument(postObj).toJson(QJsonDocument::Compact);
    QUrl url(QStringLiteral("https://api.tianditu.gov.cn/drive"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("postStr"), QString::fromUtf8(postStr));
    query.addQueryItem(QStringLiteral("type"), QStringLiteral("search"));
    query.addQueryItem(QStringLiteral("tk"), ConfigManager::GetInstance()->tiandituKey());
    url.setQuery(query);

    QNetworkRequest req(url);
    setBusy(true);
    m_reply = m_network->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &TiandituRoutePlanner::onReplyFinished);
}

void TiandituRoutePlanner::onReplyFinished()
{
    setBusy(false);
    if (!m_reply)
        return;

    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit routeFailed(reply->errorString());
        return;
    }

    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        emit routeFailed(tr("服务器返回空数据"));
        return;
    }

    const QString routelatlon = extractRoutelatlonFromXml(data);
    if (!routelatlon.isEmpty()) {
        const QVariantList path = parseRoutelatlonString(routelatlon);
        if (path.size() >= 2) {
            emit routeReady(path);
            return;
        }
        emit routeFailed(tr("路线坐标无效或点数不足"));
        return;
    }

    if (data.startsWith('{')) {
        emit routeFailed(tr("返回为 JSON，当前版本仅解析驾车服务 XML 中的 routelatlon"));
        return;
    }

    emit routeFailed(tr("响应中未找到 routelatlon，请检查密钥与起终点坐标"));
}

void TiandituRoutePlanner::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

QString TiandituRoutePlanner::extractRoutelatlonFromXml(const QByteArray &data) const
{
    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement:
            if (xml.name() == QLatin1String("routelatlon"))
                return xml.readElementText();
            break;
        default:
            break;
        }
    }
    if (xml.hasError())
        return QString();
    return QString();
}

QVariantList TiandituRoutePlanner::parseRoutelatlonString(const QString &text) const
{
    QVariantList out;
    QString s = text.trimmed();
    while (s.endsWith(QLatin1Char(';')) || s.endsWith(QLatin1Char(' ')))
        s.chop(1);

    const QStringList segments = s.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    out.reserve(segments.size());
    for (const QString &seg : segments) {
        const QStringList ll = seg.split(QLatin1Char(','));
        if (ll.size() < 2)
            continue;
        bool okLon = false;
        bool okLat = false;
        const double lon = ll.at(0).trimmed().toDouble(&okLon);
        const double lat = ll.at(1).trimmed().toDouble(&okLat);
        if (!okLon || !okLat)
            continue;
        QVariantMap m;
        m[QStringLiteral("latitude")] = lat;
        m[QStringLiteral("longitude")] = lon;
        out.append(m);
    }
    return out;
}
