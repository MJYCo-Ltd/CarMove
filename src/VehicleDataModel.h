#ifndef VEHICLEDATAMODEL_H
#define VEHICLEDATAMODEL_H

#include <QAbstractListModel>
#include <QGeoCoordinate>
#include <QDateTime>
#include <QTimer>
#include <QHash>
#include "ExcelDataReader.h"

class VehicleDataModel : public QAbstractListModel
{
    Q_OBJECT
    
public:
    struct VehicleState {
        QString plateNumber;
        QGeoCoordinate position;
        double speed;
        int direction;
        QDateTime timestamp;
        QString color;
    };
    
    explicit VehicleDataModel(QObject *parent = nullptr);
    
    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
    void setVehicleData(const QList<ExcelDataReader::VehicleRecord>& records);
    QList<VehicleState> getVehicleStatesAtTime(const QDateTime& time);
    QDateTime getStartTime() const;
    QDateTime getEndTime() const;
    QStringList getVehicleList() const;
    
    // Performance optimization methods
    void setDataProcessingBatchSize(int batchSize) { m_batchSize = batchSize; }
    void enableTimeIndexing(bool enabled) { m_timeIndexingEnabled = enabled; }
    void clearCache() { m_stateCache.clear(); }
    
signals:
    void dataChanged();
    void dataProcessingProgress(int percentage);
    
private slots:
    void processPendingData();
    
private:
    enum Roles {
        PlateNumberRole = Qt::UserRole + 1,
        PositionRole,
        SpeedRole,
        DirectionRole,
        TimestampRole,
        ColorRole
    };
    
    // Core data
    QList<ExcelDataReader::VehicleRecord> m_vehicleRecords;
    QDateTime m_startTime;
    QDateTime m_endTime;
    
    // Performance optimization members
    QTimer* m_dataProcessingTimer;
    QList<ExcelDataReader::VehicleRecord> m_pendingRecords;
    int m_batchSize = 1000;
    bool m_timeIndexingEnabled = true;
    
    // Time-based indexing for fast lookups
    QHash<qint64, QList<int>> m_timeIndex; // timestamp -> record indices
    QHash<qint64, QList<VehicleState>> m_stateCache; // cached states by time
    
    // Helper methods
    void buildTimeIndex();
    void addToTimeIndex(const ExcelDataReader::VehicleRecord& record, int index);
    QList<VehicleState> computeVehicleStatesAtTime(const QDateTime& time);
    qint64 timeToKey(const QDateTime& time) const;
    void calculateTimeRange();
};

#endif // VEHICLEDATAMODEL_H