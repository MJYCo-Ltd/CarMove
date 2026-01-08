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
 * @brief 使用用户定义的列映射读取和解析Excel格式的车辆轨迹数据
 * 
 * @see VehicleRecord
 * @see ConfigManager
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
        QString vehicleColor;     // 车辆颜色 (如: 黄色)
        double speed;            // 速度 km/h (如: 64)
        double longitude;        // 经度 (如: 116.483059)
        double latitude;         // 纬度 (如: 39.564428)
        int direction;           // 方向角度 0-360° (如: 172)
        double distance;         // 距离 (如: 11)
        QDateTime timestamp;     // 上报时间 (如: 2025-05-23 13:42:07)
        QString totalMileage;    // 总里程 (如: 55511)
        
        // 坐标访问方法
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
        
        // 内存管理：使用移动语义
        VehicleRecord() = default;
        VehicleRecord(const VehicleRecord&) = default;
        VehicleRecord(VehicleRecord&&) = default;
        VehicleRecord& operator=(const VehicleRecord&) = default;
        VehicleRecord& operator=(VehicleRecord&&) = default;
    };
    
    explicit ExcelDataReader(QObject *parent = nullptr);
    
    /**
     * @brief 使用列映射配置加载Excel文件
     * 
     * @note 加载成功后会发射 dataLoaded 信号，加载过程中会发射 loadingProgress 信号
     */
    bool loadExcelFile(const QString& filePath);
    
    // 数据访问方法
    /**
     * @brief 获取已加载的所有车辆记录
     * @return 车辆记录列表，如果没有加载数据则返回空列表
     */
    QList<VehicleRecord> getVehicleData() const;
    
    /**
     * @brief 获取已加载数据中的唯一车牌号列表
     * @return 车牌号列表，如果没有加载数据则返回空列表
     */
    QStringList getUniqueVehicles() const;
    
    /**
     * @brief 获取指定车牌号的所有记录
     * @param plateNumber 车牌号
     * @return 该车牌号的所有记录，如果没有找到则返回空列表
     */
    QList<VehicleRecord> getVehicleRecords(const QString& plateNumber) const;
    
signals:
    void dataLoaded(const QList<VehicleRecord>& records);
    void loadingProgress(int percentage);
    void errorOccurred(const QString& error);
    void columnMappingValidated(bool isValid, const QStringList& errors);
    
private:
    QList<VehicleRecord> m_vehicleData;
    
    // 核心解析方法
    bool parseDataRowWithMapping(QXlsx::Document& xlsx, int row,
                                VehicleRecord& record, QString& errorMessage);
    QDateTime parseTimestamp(const QVariant& value) const;
    QVariant parseAndValidateField(const QVariant& cellValue, const QString& dataType, 
                                  const QString& fieldName, QString& errorMessage) const;
};

#endif // EXCELDATAREADER_H
