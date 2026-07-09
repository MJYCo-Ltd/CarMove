#ifndef TIANDITU_ROUTE_PLANNER_H
#define TIANDITU_ROUTE_PLANNER_H

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>

/**
 * @brief 天地图驾车路线 Web 服务（drive）封装
 * @see http://lbs.tianditu.gov.cn/server/drive.html
 */
class TiandituRoutePlanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    explicit TiandituRoutePlanner(QObject *parent = nullptr);
    ~TiandituRoutePlanner() override;

    bool isBusy() const { return m_busy; }

    /// 规划路线：起点、终点为 WGS84 经纬度（经度、纬度），与天地图文档一致
    /// @param style 0 最快，1 最短，2 避开高速，3 步行
    /// @param mid 途经点，格式 "lng,lat;lng,lat"，可为空
    Q_INVOKABLE void requestRoute(double origLon, double origLat, double destLon, double destLat,
                                  int style, const QString &mid = QString());

signals:
    void routeReady(const QVariantList &pathPoints);
    void routeFailed(const QString &errorMessage);
    void busyChanged();

private slots:
    void onReplyFinished();

private:
    void setBusy(bool busy);
    void sendDriveRequest(const QJsonObject &postObj);
    QVariantList parseRoutelatlonString(const QString &text) const;
    QString extractRoutelatlonFromXml(const QByteArray &data) const;

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    bool m_busy = false;
};

#endif
