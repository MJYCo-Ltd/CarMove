#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QGeoCoordinate>
#include <QString>
#include <QStringList>
#include <QQmlEngine>
#include <QVariantMap>
#include <QList>
#include <QDateTime>

/**
 * @class ConfigManager
 * @brief 统一配置管理器，管理地图配置和Excel列映射配置
 */
class ConfigManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(int mapTypeIndex READ mapTypeIndex WRITE setMapTypeIndex NOTIFY mapTypeIndexChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(QGeoCoordinate mapCenter READ mapCenter WRITE setMapCenter NOTIFY mapCenterChanged)
    Q_PROPERTY(bool coordinateConversionEnabled READ coordinateConversionEnabled WRITE setCoordinateConversionEnabled NOTIFY coordinateConversionEnabledChanged)
    
public:
    /**
     * @struct FieldMapping
     * @brief 字段映射结构体，定义单个字段的映射信息
     */
    struct FieldMapping {
        QString fieldName;          // 字段名称，例如: "经度", "纬度", "车牌号"
        int columnIndex;           // 基于1的Excel列索引 (0 = 未映射)
        bool isRequired;           // 经度、纬度、时间为true
        QString displayName;       // 用户友好的字段名称
        QString dataType;          // "text", "number", "datetime"
        
        // 默认构造函数
        FieldMapping() : columnIndex(0), isRequired(false) {}
        
        // 完整构造函数
        FieldMapping(const QString& field, int column, bool required, 
                    const QString& display, const QString& type)
            : fieldName(field), columnIndex(column), isRequired(required),
              displayName(display), dataType(type) {}
        
        // 验证字段映射是否有效
        bool isValid() const {
            return !fieldName.isEmpty() && 
                   !displayName.isEmpty() && 
                   !dataType.isEmpty() &&
                   (columnIndex > 0 || !isRequired); // 必需字段必须有列映射
        }
        
        // 检查是否已映射
        bool isMapped() const {
            return columnIndex > 0;
        }
        
        // 比较操作符
        bool operator==(const FieldMapping& other) const {
            return fieldName == other.fieldName &&
                   columnIndex == other.columnIndex &&
                   isRequired == other.isRequired &&
                   displayName == other.displayName &&
                   dataType == other.dataType;
        }
    };
    
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager();
    static ConfigManager* GetInstance();
    
    // Map property getters
    int mapTypeIndex() const { return m_mapTypeIndex; }
    double zoomLevel() const { return m_zoomLevel; }
    QGeoCoordinate mapCenter() const { return m_mapCenter; }
    bool coordinateConversionEnabled() const { return m_coordinateConversionEnabled; }
    
    // Map property setters
    void setMapTypeIndex(int index);
    void setZoomLevel(double level);
    void setMapCenter(const QGeoCoordinate& center);
    void setCoordinateConversionEnabled(bool enabled);
    
    // Excel column mapping methods
    int getExcelDataStartRow() const { return m_excelDataStartRow; }
    void setExcelDataStartRow(int row) { m_excelDataStartRow = row; }
    QList<FieldMapping> getExcelFieldMappings() const { return m_excelFieldMappings; }
    void setExcelFieldMappings(const QList<FieldMapping>& mappings) { m_excelFieldMappings = mappings; }
    
    // Field mapping management
    void addFieldMapping(const QString& fieldName, int columnIndex, bool isRequired,
                        const QString& displayName, const QString& dataType);
    void removeFieldMapping(const QString& fieldName);
    FieldMapping getFieldMapping(const QString& fieldName) const;
    int getColumnForField(const QString& fieldName) const;
    bool isFieldMapped(const QString& fieldName) const;
    
    // Validation methods
    bool isValid() const;
    QStringList getValidationErrors() const;
    QStringList getRequiredFields() const;
    
    // Invokable methods for QML
    Q_INVOKABLE void saveMapState();
    Q_INVOKABLE void loadMapState();
    Q_INVOKABLE void resetToDefaults();
    Q_INVOKABLE void saveExcelColumnMapping(int dataStartRow, const QVariantMap& fieldMappings);
    Q_INVOKABLE QVariantMap loadExcelColumnMapping();
    Q_INVOKABLE QVariantMap getExcelFieldMappingsVariant() const;
    Q_INVOKABLE void createDefaultExcelMapping();
    
    // Static helper methods
    static QStringList getStandardFieldNames();
    static QStringList getRequiredFieldNames();
    
signals:
    void mapTypeIndexChanged();
    void zoomLevelChanged();
    void mapCenterChanged();
    void coordinateConversionEnabledChanged();
    void mapStateLoaded();
    void excelColumnMappingChanged();
    
private:
    void loadSettings();
    void saveSettings();
    void loadExcelSettings();
    void saveExcelSettings();
    
    // Map configuration properties
    int m_mapTypeIndex;
    double m_zoomLevel;
    QGeoCoordinate m_mapCenter;
    bool m_coordinateConversionEnabled;
    
    // Excel configuration properties
    int m_excelDataStartRow;
    QList<FieldMapping> m_excelFieldMappings;
    
    // Settings instance
    QSettings* m_settings;
    
    // Default values
    static const int DEFAULT_MAP_TYPE_INDEX = 0;
    static inline const double DEFAULT_ZOOM_LEVEL = 12.0;
    static inline const double DEFAULT_LATITUDE = 39.9;
    static inline const double DEFAULT_LONGITUDE = 116.4;
    static const bool DEFAULT_COORDINATE_CONVERSION = false;
    static const int DEFAULT_EXCEL_DATA_START_ROW = 2;
    static ConfigManager* m_pManager;
};

// 注册元类型以支持QML和信号槽
Q_DECLARE_METATYPE(ConfigManager::FieldMapping)

#endif // CONFIGMANAGER_H
