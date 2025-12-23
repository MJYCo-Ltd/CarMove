#ifndef EXCELDATAREADER_H
#define EXCELDATAREADER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QMap>
#include <QVariant>

// Forward declarations
namespace QXlsx {
    class Document;
    class Worksheet;
}

/**
 * @class ExcelDataReader
 * @brief 读取和解析Excel格式的车辆轨迹数据
 * 
 * ExcelDataReader类使用QXlsx库读取Excel文件，提取车辆位置、速度、方向等数据。
 * 支持灵活的列名匹配，能够处理不同格式的Excel文件。
 * 
 * 主要功能：
 * - 读取Excel文件并解析车辆数据
 * - 验证数据有效性（经纬度范围、时间格式等）
 * - 按时间顺序排序数据
 * - 提供进度报告和错误处理
 * - 支持多种时间戳格式
 * - 获取数据统计信息
 * 
 * 使用示例：
 * @code
 * ExcelDataReader reader;
 * connect(&reader, &ExcelDataReader::dataLoaded, [](const QList<VehicleRecord>& records) {
 *     // Process loaded records
 * });
 * reader.loadExcelFile("carData/vehicle-2025-05-23.xlsx");
 * @endcode
 * 
 * @see VehicleRecord
 */
class ExcelDataReader : public QObject
{
    Q_OBJECT
    
public:
    /**
     * @struct VehicleRecord
     * @brief 车辆记录数据结构，优化内存使用
     * 
     * 包含车辆的位置、速度、方向和时间信息。
     * 使用紧凑的数据布局以减少内存占用。
     */
    struct VehicleRecord {
        QString plateNumber;      // 车牌号 (如: 冀JY8706)
        QString vehicleColor;     // 车辆颜色 (如: 黄色) - 使用intern string减少内存
        double speed;            // 速度 km/h (如: 64)
        double longitude;        // 经度 (如: 116.483059)
        double latitude;         // 纬度 (如: 39.564428)
        int direction;           // 方向角度 0-360° (如: 172)
        double distance;         // 距离 (如: 11)
        QDateTime timestamp;     // 上报时间 (如: 2025-05-23 13:42:07)
        QString totalMileage;    // 总里程 (如: 55511) - 使用intern string减少内存
        
        // 优化的坐标访问方法
        QGeoCoordinate coordinate() const { 
            return QGeoCoordinate(latitude, longitude); 
        }
        
        // 验证数据有效性
        bool isValid() const {
            return !plateNumber.isEmpty() &&
                   longitude >= -180.0 && longitude <= 180.0 &&
                   latitude >= -90.0 && latitude <= 90.0 &&
                   direction >= 0 && direction <= 360 &&
                   speed >= 0.0 &&
                   timestamp.isValid();
        }
        
        // 验证坐标是否在中国境内的合理范围
        bool isInChinaRange() const {
            // 中国大陆的大致坐标范围
            return longitude >= 73.0 && longitude <= 135.0 &&
                   latitude >= 18.0 && latitude <= 54.0;
        }
        
        // 内存优化：使用移动语义
        VehicleRecord() = default;
        VehicleRecord(const VehicleRecord&) = default;
        VehicleRecord(VehicleRecord&&) = default;
        VehicleRecord& operator=(const VehicleRecord&) = default;
        VehicleRecord& operator=(VehicleRecord&&) = default;
    };
    
    explicit ExcelDataReader(QObject *parent = nullptr);
    
    bool loadExcelFile(const QString& filePath);
    QList<VehicleRecord> getVehicleData() const;
    QStringList getUniqueVehicles() const;
    QList<VehicleRecord> getVehicleRecords(const QString& plateNumber) const;  // 获取特定车辆的记录
    QString getDefaultDataPath() const { return "carData"; }
    QStringList getAvailableDataFiles() const;
    
    // Performance optimization methods
    void setMemoryOptimizationEnabled(bool enabled) { m_memoryOptimizationEnabled = enabled; }
    void setBatchProcessingSize(int size) { m_batchSize = size; }
    void clearCache() { m_vehicleData.clear(); m_vehicleData.squeeze(); }
    
    // 获取数据统计信息
    struct DataStatistics {
        int totalRecords;
        int uniqueVehicles;
        QDateTime earliestTime;
        QDateTime latestTime;
        double minSpeed;
        double maxSpeed;
        QStringList vehicleColors;
    };
    DataStatistics getDataStatistics() const;
    
signals:
    void dataLoaded(const QList<VehicleRecord>& records);
    void loadingProgress(int percentage);
    void errorOccurred(const QString& error);
    
private:
    QList<VehicleRecord> m_vehicleData;
    
    // Performance optimization members
    bool m_memoryOptimizationEnabled = true;
    int m_batchSize = 1000;
    QHash<QString, QString> m_stringInternPool; // String interning for memory optimization
    
    // Helper methods for parsing Excel data
    bool parseHeaderRow(QXlsx::Worksheet *pSheet, QMap<QString, int>& columnMap);
    bool parseDataRow(QXlsx::Document& xlsx, int row, const QMap<QString, int>& columnMap, VehicleRecord& record, QString& errorMessage);
    QDateTime parseTimestamp(const QVariant& value);
    
    // Performance optimization methods
    QString internString(const QString& str);
    void optimizeMemoryUsage();
};

#endif // EXCELDATAREADER_H
