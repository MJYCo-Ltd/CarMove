#ifndef EXCELDATAREADER_H
#define EXCELDATAREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QList>

/**
 * @class ExcelDataReader
 * @brief 读取 Excel 车辆轨迹数据，按文件大小自动选择后端（<100MB: QXlsx，>=100MB: xlsxio）
 *
 * @see VehicleRecord
 * @see ConfigManager
 */
class ExcelDataReader : public QObject
{
    Q_OBJECT

public:
    struct VehicleRecord {
        QString plateNumber;
        QString vehicleColor;
        double speed = 0.0;
        double longitude = 0.0;
        double latitude = 0.0;
        int direction = 0;
        double distance = 0.0;
        QDateTime timestamp;
        QString totalMileage;

        QGeoCoordinate coordinate() const
        {
            return QGeoCoordinate(latitude, longitude);
        }

        bool isValid() const
        {
            return !plateNumber.isEmpty() && longitude >= -180.0 && longitude <= 180.0
                   && latitude >= -90.0 && latitude <= 90.0 && direction >= 0 && direction <= 360
                   && speed >= 0.0 && timestamp.isValid();
        }

        bool isInChinaRange() const
        {
            return longitude >= 73.0 && longitude <= 135.0 && latitude >= 18.0 && latitude <= 54.0;
        }
    };

    explicit ExcelDataReader(QObject* parent = nullptr);

    bool loadExcelFile(const QString& filePath);
    QList<VehicleRecord> getVehicleData() const;

signals:
    void dataLoaded(const QList<VehicleRecord>& records);
    void loadingProgress(int percentage);
    void errorOccurred(const QString& error);

private:
    bool finalizeLoadedData(const QString& fileName);

    QList<VehicleRecord> m_vehicleData;
};

#endif // EXCELDATAREADER_H
