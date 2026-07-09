#ifndef EXCELTRAJECTORYMANAGER_H
#define EXCELTRAJECTORYMANAGER_H

#include "Domain/TrajectoryTypes.h"

#include <QDate>
#include <QObject>
#include <QString>

class ExcelParserManager;

/**
 * @brief Excel 轨迹数据管理器：文件夹扫描与轨迹文件读取。
 */
class ExcelTrajectoryManager : public QObject
{
    Q_OBJECT

public:
    struct TrajectoryLoadResult {
        QList<TrajectoryPoint> points;
        QString errorMessage;
        bool success = false;
    };

    explicit ExcelTrajectoryManager(QObject* parent = nullptr);

    void scanFolder(const QString& folderPath);
    QList<VehicleSummary> vehicleList() const { return m_vehicleList; }

    TrajectoryLoadResult loadTrajectory(const VehicleSummary& vehicle,
                                        const QString& plateNumber,
                                        const QDate& startDate = {},
                                        const QDate& endDate = {},
                                        bool hasDateRange = false);

signals:
    void scanCompleted(const QList<VehicleSummary>& vehicles);
    void scanProgress(int percentage);
    void scanError(const QString& error);
    void loadProgress(int percentage);

private:
    ExcelParserManager* m_parser = nullptr;
    QList<VehicleSummary> m_vehicleList;
};

#endif // EXCELTRAJECTORYMANAGER_H
