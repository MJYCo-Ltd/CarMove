#ifndef TRAJECTORYDATAMANAGER_H
#define TRAJECTORYDATAMANAGER_H

#include "Domain/TrajectoryTypes.h"

#include <QDate>
#include <QObject>
#include <QString>

class PostGisDataManager;
class ExcelTrajectoryManager;

/**
 * @brief 轨迹数据源管理器：统一 Excel 文件夹与 PostGIS 两种后端。
 *
 * 外部只与本类交互；读写分别委托 ExcelTrajectoryManager / PostGisDataManager。
 */
class TrajectoryDataManager : public QObject
{
    Q_OBJECT

public:
    using VehicleInfo = VehicleSummary;

    struct TrajectoryLoadRequest {
        QString plateNumber;
        QDate startDate;
        QDate endDate;
        bool preserveAllPoints = false;
        bool hasDateRange = false;
    };

    struct TrajectoryLoadResult {
        QList<TrajectoryPoint> records;
        QString errorMessage;
        bool success = false;
    };

    explicit TrajectoryDataManager(QObject* parent = nullptr);
    ~TrajectoryDataManager() override;

    QString sourceMode() const;
    bool useDatabaseSource() const;
    bool isReady() const;
    QString sourceDescription() const;
    bool supportsDateRangeQuery() const;

    void setSourceMode(const QString& mode);
    void scanFolder(const QString& folderPath);
    void refreshDatabaseSource();

    QList<VehicleSummary> vehicleList() const { return m_vehicleList; }

    TrajectoryLoadResult loadTrajectory(const TrajectoryLoadRequest& request);

    /// 应用退出前释放 PostGIS 连接，避免进程析构阶段卡住
    void releaseDatabaseConnection();

signals:
    void sourceModeChanged();
    void readyChanged();
    void sourceDescriptionChanged();
    void scanCompleted(const QList<VehicleSummary>& vehicles);
    void scanError(const QString& error);
    void scanProgress(int percentage);
    void loadProgress(int percentage);

private slots:
    void onExcelScanCompleted(const QList<VehicleSummary>& vehicles);
    void onExcelScanError(const QString& error);
    void onExcelScanProgress(int percentage);
    void onExcelLoadProgress(int percentage);

private:
    void applySourceMode();
    void clearSourceState();
    bool connectDatabaseSource(QString& errorMessage);
    void disconnectDatabaseSource();
    TrajectoryLoadResult loadTrajectoryFromFolder(const TrajectoryLoadRequest& request);
    TrajectoryLoadResult loadTrajectoryFromDatabase(const TrajectoryLoadRequest& request);

    PostGisDataManager* m_postGisData = nullptr;
    ExcelTrajectoryManager* m_excelTrajectory = nullptr;

    QString m_sourceMode;
    QString m_folderPath;
    QString m_sourceDescription;
    bool m_ready = false;

    QList<VehicleSummary> m_vehicleList;
};

#endif // TRAJECTORYDATAMANAGER_H
