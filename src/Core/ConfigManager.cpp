#include "Core/ConfigManager.h"
#include <QCoreApplication>
#include <QDir>
#include <algorithm>

ConfigManager* ConfigManager::m_pManager{};

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_mapTypeIndex(DEFAULT_MAP_TYPE_INDEX)
    , m_zoomLevel(DEFAULT_ZOOM_LEVEL)
    , m_mapCenter(QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE))
    , m_coordinateConversionEnabled(DEFAULT_COORDINATE_CONVERSION)
    , m_tiandituKey(DEFAULT_TIANDITU_KEY)
    , m_trajectorySourceMode(QString::fromLatin1(DEFAULT_TRAJECTORY_SOURCE_MODE))
    , m_dbHost(QString::fromLatin1(DEFAULT_DB_HOST))
    , m_dbPort(DEFAULT_DB_PORT)
    , m_dbName(QString::fromLatin1(DEFAULT_DB_NAME))
    , m_dbUser(QString::fromLatin1(DEFAULT_DB_USER))
    , m_dbPassword(QString::fromLatin1(DEFAULT_DB_PASSWORD))
    , m_dbSchema(QString::fromLatin1(DEFAULT_DB_SCHEMA))
    , m_dbTrajectoryTable(QString::fromLatin1(DEFAULT_DB_TRAJECTORY_TABLE))
    , m_dbTrajectoryDaysTable(QString::fromLatin1(DEFAULT_DB_TRAJECTORY_DAYS_TABLE))
    , m_dbVehiclesTable(QString::fromLatin1(DEFAULT_DB_VEHICLES_TABLE))
    , m_excelDataStartRow(DEFAULT_EXCEL_DATA_START_ROW)
    , m_settings(nullptr)
{
    // 配置文件路径与 exe 相同
    QString configPath = QCoreApplication::applicationDirPath();
    
    // 初始化QSettings
    QString configFile = configPath + "/CarMoveTracker.ini";
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);
    
    // 加载保存的设置
    loadSettings();
    loadExcelSettings();
    loadTrajectorySourceSettings();
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

void ConfigManager::setTiandituKey(const QString& key)
{
    if (m_tiandituKey != key) {
        m_tiandituKey = key;
        emit tiandituKeyChanged();
    }
}

void ConfigManager::setTrajectorySourceMode(const QString& mode)
{
    const QString normalized = mode == QStringLiteral("database") ? QStringLiteral("database")
                                                                  : QStringLiteral("folder");
    if (m_trajectorySourceMode == normalized) {
        return;
    }
    m_trajectorySourceMode = normalized;
    saveTrajectorySourceSettings();
    emit trajectorySourceModeChanged();
}

void ConfigManager::setDbHost(const QString& host)
{
    if (m_dbHost != host) {
        m_dbHost = host;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbPort(int port)
{
    Q_UNUSED(port);
    // 端口写死在代码中，不接受外部修改
    if (m_dbPort != DEFAULT_DB_PORT) {
        m_dbPort = DEFAULT_DB_PORT;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbName(const QString& name)
{
    if (m_dbName != name) {
        m_dbName = name;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbUser(const QString& user)
{
    if (m_dbUser != user) {
        m_dbUser = user;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbPassword(const QString& password)
{
    Q_UNUSED(password);
    // 密码写死在代码中，不接受外部修改
    const QString hardcoded = QString::fromLatin1(DEFAULT_DB_PASSWORD);
    if (m_dbPassword != hardcoded) {
        m_dbPassword = hardcoded;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbSchema(const QString& schema)
{
    if (m_dbSchema != schema) {
        m_dbSchema = schema;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbTrajectoryTable(const QString& table)
{
    if (m_dbTrajectoryTable != table) {
        m_dbTrajectoryTable = table;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbTrajectoryDaysTable(const QString& table)
{
    if (m_dbTrajectoryDaysTable != table) {
        m_dbTrajectoryDaysTable = table;
        emit postGisSettingsChanged();
    }
}

void ConfigManager::setDbVehiclesTable(const QString& table)
{
    if (m_dbVehiclesTable != table) {
        m_dbVehiclesTable = table;
        emit postGisSettingsChanged();
    }
}

PostGisDatabaseConfig ConfigManager::postGisDatabaseConfig() const
{
    PostGisDatabaseConfig config;
    config.host = m_dbHost;
    config.port = m_dbPort;
    config.database = m_dbName;
    config.username = m_dbUser;
    config.password = m_dbPassword;
    config.schema = m_dbSchema;
    config.trajectoryTable = m_dbTrajectoryTable;
    config.trajectoryDaysTable = m_dbTrajectoryDaysTable;
    config.vehiclesTable = m_dbVehiclesTable;
    return config;
}

void ConfigManager::savePostGisSettings()
{
    saveTrajectorySourceSettings();
    m_settings->sync();
}

void ConfigManager::loadPostGisSettings()
{
    loadTrajectorySourceSettings();
    emit trajectorySourceModeChanged();
    emit postGisSettingsChanged();
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

void ConfigManager::persistTargetArea(double latitude, double longitude, const QString& name)
{
    m_settings->beginGroup("MapSettings");
    m_settings->setValue("targetAreaLatitude", latitude);
    m_settings->setValue("targetAreaLongitude", longitude);
    m_settings->setValue("targetAreaName", name);
    m_settings->endGroup();
    m_settings->sync();

    const QGeoCoordinate center(latitude, longitude);
    if (center.isValid() && m_mapCenter != center) {
        m_mapCenter = center;
        emit mapCenterChanged();
    }
}

QVariantMap ConfigManager::loadPersistedTargetArea()
{
    QVariantMap out;
    m_settings->beginGroup("MapSettings");
    if (m_settings->contains("targetAreaLatitude"))
        out[QStringLiteral("latitude")] = m_settings->value("targetAreaLatitude");
    if (m_settings->contains("targetAreaLongitude"))
        out[QStringLiteral("longitude")] = m_settings->value("targetAreaLongitude");
    if (m_settings->contains("targetAreaName"))
        out[QStringLiteral("name")] = m_settings->value("targetAreaName");
    m_settings->endGroup();
    return out;
}

void ConfigManager::resetToDefaults()
{
    m_mapTypeIndex = DEFAULT_MAP_TYPE_INDEX;
    m_zoomLevel = DEFAULT_ZOOM_LEVEL;
    m_mapCenter = QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE);
    m_coordinateConversionEnabled = DEFAULT_COORDINATE_CONVERSION;

    m_settings->beginGroup("MapSettings");
    m_settings->remove("targetAreaLatitude");
    m_settings->remove("targetAreaLongitude");
    m_settings->remove("targetAreaName");
    m_settings->remove("centerLatitude");
    m_settings->remove("centerLongitude");
    m_settings->endGroup();

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

    if (m_settings->contains("targetAreaLatitude") && m_settings->contains("targetAreaLongitude")) {
        m_mapCenter = QGeoCoordinate(m_settings->value("targetAreaLatitude").toDouble(),
                                     m_settings->value("targetAreaLongitude").toDouble());
    } else {
        m_mapCenter = QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE);
    }

    m_settings->remove("centerLatitude");
    m_settings->remove("centerLongitude");

    m_coordinateConversionEnabled = m_settings->value("coordinateConversionEnabled", DEFAULT_COORDINATE_CONVERSION).toBool();
    m_tiandituKey = m_settings->value("tiandituKey", DEFAULT_TIANDITU_KEY).toString();

    m_settings->endGroup();
}

void ConfigManager::saveSettings()
{
    m_settings->beginGroup("MapSettings");
    
    m_settings->setValue("mapTypeIndex", m_mapTypeIndex);
    m_settings->setValue("zoomLevel", m_zoomLevel);
    m_settings->setValue("coordinateConversionEnabled", m_coordinateConversionEnabled);
    m_settings->setValue("tiandituKey", m_tiandituKey);

    m_settings->endGroup();
    
    // 同时保存 Excel 设置
    saveExcelSettings();
    saveTrajectorySourceSettings();
    
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

void ConfigManager::saveTrajectorySourceSettings()
{
    m_settings->beginGroup("TrajectorySource");
    m_settings->setValue("mode", m_trajectorySourceMode);
    m_settings->endGroup();

    m_settings->beginGroup("PostGISDatabase");
    m_settings->setValue("host", m_dbHost);
    m_settings->remove("port");      // 端口写死在代码中，不再写入 ini
    m_settings->setValue("database", m_dbName);
    m_settings->setValue("username", m_dbUser);
    m_settings->remove("password");  // 密码写死在代码中，不再写入 ini
    m_settings->setValue("schema", m_dbSchema);
    m_settings->setValue("trajectoryTable", m_dbTrajectoryTable);
    m_settings->setValue("trajectoryDaysTable", m_dbTrajectoryDaysTable);
    m_settings->setValue("vehiclesTable", m_dbVehiclesTable);
    m_settings->endGroup();
}

void ConfigManager::loadTrajectorySourceSettings()
{
    m_settings->beginGroup("TrajectorySource");
    m_trajectorySourceMode =
        m_settings->value("mode", QString::fromLatin1(DEFAULT_TRAJECTORY_SOURCE_MODE)).toString();
    if (m_trajectorySourceMode != QStringLiteral("database")) {
        m_trajectorySourceMode = QStringLiteral("folder");
    }
    m_settings->endGroup();

    m_settings->beginGroup("PostGISDatabase");
    m_dbHost = m_settings->value("host", QString::fromLatin1(DEFAULT_DB_HOST)).toString();
    m_dbPort = DEFAULT_DB_PORT;
    m_dbName = m_settings->value("database", QString::fromLatin1(DEFAULT_DB_NAME)).toString();
    m_dbUser = m_settings->value("username", QString::fromLatin1(DEFAULT_DB_USER)).toString();
    m_dbPassword = QString::fromLatin1(DEFAULT_DB_PASSWORD);
    m_dbSchema = m_settings->value("schema", QString::fromLatin1(DEFAULT_DB_SCHEMA)).toString();
    m_dbTrajectoryTable =
        m_settings->value("trajectoryTable", QString::fromLatin1(DEFAULT_DB_TRAJECTORY_TABLE)).toString();
    m_dbTrajectoryDaysTable =
        m_settings->value("trajectoryDaysTable", QString::fromLatin1(DEFAULT_DB_TRAJECTORY_DAYS_TABLE)).toString();
    m_dbVehiclesTable =
        m_settings->value("vehiclesTable", QString::fromLatin1(DEFAULT_DB_VEHICLES_TABLE)).toString();
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
