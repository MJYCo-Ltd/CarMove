#ifndef TIANDITU_GEOCODER_H
#define TIANDITU_GEOCODER_H

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QGeoCoordinate>
#include <QHash>

/**
 * @brief 天地图地名搜索 V2.0 服务封装
 * - 1.1 行政区划区域搜索服务 (queryType=12)，配合 AdminCode.csv 国标码
 * @see http://lbs.tianditu.gov.cn/server/search2.html
 */
class TiandituGeocoder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)

public:
    explicit TiandituGeocoder(QObject *parent = nullptr);
    ~TiandituGeocoder();

    bool isBusy() const { return m_busy; }

    /// 1.1 行政区划区域搜索：在指定行政区(specify 国标码)内搜索关键字
    /// @param keyWord 搜索关键字（如：商厦、医院）
    /// @param specifyAdminCode 9 位国标码（见 AdminCode.csv），如北京 156110000
    Q_INVOKABLE void searchInAdminRegion(const QString &keyWord, const QString &specifyAdminCode);

    /// 根据行政区名称从 AdminCode.csv 查找国标码，未找到返回空字符串
    Q_INVOKABLE QString adminCodeForName(const QString &adminName) const;

    /// 已加载的行政区名称列表（用于下拉等），按 CSV 顺序
    Q_INVOKABLE QStringList adminRegionNames() const;

    static constexpr const char *defaultKey = "";

signals:
    void geocodeSucceeded(double latitude, double longitude, const QString &name, const QString &address);
    void geocodeFailed(const QString &errorMessage);
    void busyChanged();

private slots:
    void onReplyFinished();

private:
    void setBusy(bool busy);
    void loadAdminCodeCsv();
    bool parseAdminSearchReply(const QByteArray &json, double &outLat, double &outLon,
                               QString &outName, QString &outAddress);
    bool parseLonLat(const QString &lonlat, double &outLat, double &outLon);

    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
    bool m_busy = false;

    /// 行政区名称 -> 9 位国标码（来自 AdminCode.csv）
    QHash<QString, QString> m_adminNameToCode;
    QStringList m_adminNames;
};

#endif // TIANDITU_GEOCODER_H
