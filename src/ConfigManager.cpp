#include "ConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <algorithm>

ConfigManager* ConfigManager::m_pManager{};

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_mapTypeIndex(DEFAULT_MAP_TYPE_INDEX)
    , m_zoomLevel(DEFAULT_ZOOM_LEVEL)
    , m_mapCenter(QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE))
    , m_coordinateConversionEnabled(DEFAULT_COORDINATE_CONVERSION)
    , m_excelDataStartRow(DEFAULT_EXCEL_DATA_START_ROW)
    , m_settings(nullptr)
{
    // 创建配置文件路径
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir configDir(configPath);
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
    
    // 初始化QSettings
    QString configFile = configPath + "/CarMoveTracker.ini";
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);
    
    // 加载保存的设置
    loadSettings();
    loadExcelSettings();
}

ConfigManager::~ConfigManager()
{
    // 保存当前设置
    saveSettings();
}

ConfigManager *ConfigManager::GetInstance()
{
    if(nullptr == m_pManager){
        m_pManager = new ConfigManager;
    }

    return(m_pManager);
}

void ConfigManager::setMapTypeIndex(int index)
{
    if (m_mapTypeIndex != index) {
        m_mapTypeIndex = index;
        emit mapTypeIndexChanged();
    }
}

void ConfigManager::setZoomLevel(double level)
{
    if (qAbs(m_zoomLevel - level) > 0.01) { // 避免浮点数精度问题
        m_zoomLevel = level;
        emit zoomLevelChanged();
    }
}

void ConfigManager::setMapCenter(const QGeoCoordinate& center)
{
    if (m_mapCenter != center) {
        m_mapCenter = center;
        emit mapCenterChanged();
    }
}

void ConfigManager::setCoordinateConversionEnabled(bool enabled)
{
    if (m_coordinateConversionEnabled != enabled) {
        m_coordinateConversionEnabled = enabled;
        emit coordinateConversionEnabledChanged();
    }
}

void ConfigManager::addFieldMapping(const QString& fieldName, int columnIndex, bool isRequired,
                                   const QString& displayName, const QString& dataType)
{
    FieldMapping mapping(fieldName, columnIndex, isRequired, displayName, dataType);
    
    // 移除已存在的同名字段映射
    removeFieldMapping(fieldName);
    
    // 添加新的映射
    m_excelFieldMappings.append(mapping);
    
    emit excelColumnMappingChanged();
}

void ConfigManager::removeFieldMapping(const QString& fieldName)
{
    auto it = std::remove_if(m_excelFieldMappings.begin(), m_excelFieldMappings.end(),
                            [&fieldName](const FieldMapping& mapping) {
                                return mapping.fieldName == fieldName;
                            });
    
    if (it != m_excelFieldMappings.end()) {
        m_excelFieldMappings.erase(it, m_excelFieldMappings.end());
        emit excelColumnMappingChanged();
    }
}

ConfigManager::FieldMapping ConfigManager::getFieldMapping(const QString& fieldName) const
{
    for (const auto& mapping : m_excelFieldMappings) {
        if (mapping.fieldName == fieldName) {
            return mapping;
        }
    }
    return FieldMapping(); // 返回默认构造的映射
}

int ConfigManager::getColumnForField(const QString& fieldName) const
{
    for (const auto& mapping : m_excelFieldMappings) {
        if (mapping.fieldName == fieldName) {
            return mapping.columnIndex;
        }
    }
    return 0; // 未映射
}

bool ConfigManager::isFieldMapped(const QString& fieldName) const
{
    return getColumnForField(fieldName) > 0;
}

bool ConfigManager::isValid() const
{
    return getValidationErrors().isEmpty();
}

QStringList ConfigManager::getValidationErrors() const
{
    QStringList errors;
    
    // 检查数据起始行是否有效
    if (m_excelDataStartRow < 1) {
        errors << "数据起始行必须大于0";
    }
    
    // 检查所有必需字段是否已映射
    QStringList requiredFields = getRequiredFieldNames();
    for (const QString& requiredField : requiredFields) {
        bool found = false;
        for (const FieldMapping& mapping : m_excelFieldMappings) {
            if (mapping.fieldName == requiredField && mapping.isMapped()) {
                found = true;
                break;
            }
        }
        if (!found) {
            errors << QString("必需字段 '%1' 未映射").arg(requiredField);
        }
    }
    
    // 检查字段映射的有效性
    for (const FieldMapping& mapping : m_excelFieldMappings) {
        if (!mapping.isValid()) {
            if (mapping.fieldName.isEmpty()) {
                errors << "字段名称不能为空";
            }
            if (mapping.displayName.isEmpty()) {
                errors << QString("字段 '%1' 的显示名称不能为空").arg(mapping.fieldName);
            }
            if (mapping.dataType.isEmpty()) {
                errors << QString("字段 '%1' 的数据类型不能为空").arg(mapping.fieldName);
            }
            if (mapping.isRequired && !mapping.isMapped()) {
                errors << QString("必需字段 '%1' 必须映射到Excel列").arg(mapping.fieldName);
            }
        }
    }
    
    // 检查列冲突
    QMap<int, QStringList> columnUsage;
    for (const FieldMapping& mapping : m_excelFieldMappings) {
        if (mapping.isMapped()) {
            columnUsage[mapping.columnIndex].append(mapping.fieldName);
        }
    }
    
    for (auto it = columnUsage.begin(); it != columnUsage.end(); ++it) {
        if (it.value().size() > 1) {
            errors << QString("列 %1 被多个字段映射: %2").arg(it.key()).arg(it.value().join(", "));
        }
    }
    
    return errors;
}

QStringList ConfigManager::getRequiredFields() const
{
    QStringList required;
    for (const FieldMapping& mapping : m_excelFieldMappings) {
        if (mapping.isRequired) {
            required.append(mapping.fieldName);
        }
    }
    return required;
}

void ConfigManager::saveMapState()
{
    saveSettings();
}

void ConfigManager::loadMapState()
{
    loadSettings();
    emit mapStateLoaded();
}

void ConfigManager::resetToDefaults()
{
    m_mapTypeIndex = DEFAULT_MAP_TYPE_INDEX;
    m_zoomLevel = DEFAULT_ZOOM_LEVEL;
    m_mapCenter = QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE);
    m_coordinateConversionEnabled = DEFAULT_COORDINATE_CONVERSION;
    
    saveSettings();
    
    emit mapTypeIndexChanged();
    emit zoomLevelChanged();
    emit mapCenterChanged();
    emit coordinateConversionEnabledChanged();
}

void ConfigManager::createDefaultExcelMapping()
{
    m_excelDataStartRow = DEFAULT_EXCEL_DATA_START_ROW;
    m_excelFieldMappings.clear();
    
    // 添加标准字段映射（未指定列，需要用户配置）
    addFieldMapping("车牌号", 0, false, "车牌号", "text");
    addFieldMapping("车牌颜色", 0, false, "车牌颜色", "text");
    addFieldMapping("速度", 0, false, "速度", "number");
    addFieldMapping("经度", 0, true, "经度", "number");
    addFieldMapping("纬度", 0, true, "纬度", "number");
    addFieldMapping("方向", 0, false, "方向", "number");
    addFieldMapping("上报时间", 0, true, "上报时间", "datetime");
    addFieldMapping("总里程", 0, false, "总里程", "text");
    
    saveExcelSettings();
    m_settings->sync();
    
    emit excelColumnMappingChanged();
}

QStringList ConfigManager::getStandardFieldNames()
{
    return QStringList{
        "车牌号", "车牌颜色", "速度", "经度", 
        "纬度", "方向", "上报时间", "总里程"
    };
}

QStringList ConfigManager::getRequiredFieldNames()
{
    return QStringList{"经度", "纬度", "上报时间"};
}

void ConfigManager::loadSettings()
{
    m_settings->beginGroup("MapSettings");
    
    m_mapTypeIndex = m_settings->value("mapTypeIndex", DEFAULT_MAP_TYPE_INDEX).toInt();
    m_zoomLevel = m_settings->value("zoomLevel", DEFAULT_ZOOM_LEVEL).toDouble();
    
    // 加载地图中心坐标
    double latitude = m_settings->value("centerLatitude", DEFAULT_LATITUDE).toDouble();
    double longitude = m_settings->value("centerLongitude", DEFAULT_LONGITUDE).toDouble();
    m_mapCenter = QGeoCoordinate(latitude, longitude);
    
    m_coordinateConversionEnabled = m_settings->value("coordinateConversionEnabled", DEFAULT_COORDINATE_CONVERSION).toBool();
    
    m_settings->endGroup();
}

void ConfigManager::saveSettings()
{
    m_settings->beginGroup("MapSettings");
    
    m_settings->setValue("mapTypeIndex", m_mapTypeIndex);
    m_settings->setValue("zoomLevel", m_zoomLevel);
    m_settings->setValue("centerLatitude", m_mapCenter.latitude());
    m_settings->setValue("centerLongitude", m_mapCenter.longitude());
    m_settings->setValue("coordinateConversionEnabled", m_coordinateConversionEnabled);
    
    m_settings->endGroup();
    
    // 同时保存 Excel 设置
    saveExcelSettings();
    
    m_settings->sync(); // 立即写入文件
}

void ConfigManager::saveExcelSettings()
{
    m_settings->beginGroup("ExcelSettings");
    
    m_settings->setValue("dataStartRow", m_excelDataStartRow);
    
    // 保存字段映射
    m_settings->beginWriteArray("fieldMappings");
    int index = 0;
    for (const auto& mapping : m_excelFieldMappings) {
        m_settings->setArrayIndex(index);
        m_settings->setValue("fieldName", mapping.fieldName);
        m_settings->setValue("columnIndex", mapping.columnIndex);
        m_settings->setValue("isRequired", mapping.isRequired);
        m_settings->setValue("displayName", mapping.displayName);
        m_settings->setValue("dataType", mapping.dataType);
        index++;
    }
    m_settings->endArray();
    
    m_settings->endGroup();
}

void ConfigManager::loadExcelSettings()
{
    m_settings->beginGroup("ExcelSettings");
    
    m_excelDataStartRow = m_settings->value("dataStartRow", DEFAULT_EXCEL_DATA_START_ROW).toInt();
    
    // 加载字段映射
    m_excelFieldMappings.clear();
    int size = m_settings->beginReadArray("fieldMappings");
    for (int i = 0; i < size; ++i) {
        m_settings->setArrayIndex(i);
        FieldMapping mapping;
        mapping.fieldName = m_settings->value("fieldName").toString();
        mapping.columnIndex = m_settings->value("columnIndex", 0).toInt();
        mapping.isRequired = m_settings->value("isRequired", false).toBool();
        mapping.displayName = m_settings->value("displayName").toString();
        mapping.dataType = m_settings->value("dataType").toString();
        
        if (!mapping.fieldName.isEmpty()) {
            m_excelFieldMappings.append(mapping);
        }
    }
    m_settings->endArray();
    
    m_settings->endGroup();
}

void ConfigManager::saveExcelColumnMapping(int dataStartRow, const QVariantMap& fieldMappings)
{
    m_excelDataStartRow = dataStartRow;
    m_excelFieldMappings.clear();
    
    // 从 QVariantMap 转换为 FieldMapping 列表
    for (auto it = fieldMappings.begin(); it != fieldMappings.end(); ++it) {
        QVariantMap fieldMap = it.value().toMap();
        FieldMapping mapping;
        mapping.fieldName = it.key();
        mapping.columnIndex = fieldMap.value("columnIndex", 0).toInt();
        mapping.isRequired = fieldMap.value("isRequired", false).toBool();
        mapping.displayName = fieldMap.value("displayName").toString();
        mapping.dataType = fieldMap.value("dataType").toString();
        m_excelFieldMappings.append(mapping);
    }
    
    saveExcelSettings();
    m_settings->sync();
    
    emit excelColumnMappingChanged();
}

QVariantMap ConfigManager::loadExcelColumnMapping()
{
    QVariantMap result;
    result["dataStartRow"] = m_excelDataStartRow;
    
    QVariantMap fieldMappings;
    for (const auto& mapping : m_excelFieldMappings) {
        QVariantMap fieldMap;
        fieldMap["columnIndex"] = mapping.columnIndex;
        fieldMap["isRequired"] = mapping.isRequired;
        fieldMap["displayName"] = mapping.displayName;
        fieldMap["dataType"] = mapping.dataType;
        fieldMappings[mapping.fieldName] = fieldMap;
    }
    result["fieldMappings"] = fieldMappings;
    
    return result;
}

QVariantMap ConfigManager::getExcelFieldMappingsVariant() const
{
    QVariantMap fieldMappings;
    for (const auto& mapping : m_excelFieldMappings) {
        QVariantMap fieldMap;
        fieldMap["columnIndex"] = mapping.columnIndex;
        fieldMap["isRequired"] = mapping.isRequired;
        fieldMap["displayName"] = mapping.displayName;
        fieldMap["dataType"] = mapping.dataType;
        fieldMappings[mapping.fieldName] = fieldMap;
    }
    return fieldMappings;
}
