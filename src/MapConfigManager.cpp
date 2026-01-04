#include "MapConfigManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

MapConfigManager::MapConfigManager(QObject *parent)
    : QObject(parent)
    , m_mapTypeIndex(DEFAULT_MAP_TYPE_INDEX)
    , m_zoomLevel(DEFAULT_ZOOM_LEVEL)
    , m_mapCenter(QGeoCoordinate(DEFAULT_LATITUDE, DEFAULT_LONGITUDE))
    , m_coordinateConversionEnabled(DEFAULT_COORDINATE_CONVERSION)
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
    
    qDebug() << "MapConfigManager: 配置文件路径:" << configFile;
    
    // 加载保存的设置
    loadSettings();
}

MapConfigManager::~MapConfigManager()
{
    // 保存当前设置
    saveSettings();
}

void MapConfigManager::setMapTypeIndex(int index)
{
    if (m_mapTypeIndex != index) {
        m_mapTypeIndex = index;
        emit mapTypeIndexChanged();
        qDebug() << "MapConfigManager: 地图类型索引更新为:" << index;
    }
}

void MapConfigManager::setZoomLevel(double level)
{
    if (qAbs(m_zoomLevel - level) > 0.01) { // 避免浮点数精度问题
        m_zoomLevel = level;
        emit zoomLevelChanged();
        qDebug() << "MapConfigManager: 缩放级别更新为:" << level;
    }
}

void MapConfigManager::setMapCenter(const QGeoCoordinate& center)
{
    if (m_mapCenter != center) {
        m_mapCenter = center;
        emit mapCenterChanged();
        qDebug() << "MapConfigManager: 地图中心更新为:" << center.latitude() << "," << center.longitude();
    }
}

void MapConfigManager::setCoordinateConversionEnabled(bool enabled)
{
    if (m_coordinateConversionEnabled != enabled) {
        m_coordinateConversionEnabled = enabled;
        emit coordinateConversionEnabledChanged();
        qDebug() << "MapConfigManager: 坐标转换状态更新为:" << (enabled ? "启用" : "禁用");
    }
}

void MapConfigManager::saveMapState()
{
    saveSettings();
    qDebug() << "MapConfigManager: 手动保存地图状态";
}

void MapConfigManager::loadMapState()
{
    loadSettings();
    emit mapStateLoaded();
    qDebug() << "MapConfigManager: 手动加载地图状态";
}

void MapConfigManager::resetToDefaults()
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
    
    qDebug() << "MapConfigManager: 重置为默认设置";
}

void MapConfigManager::loadSettings()
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
    
    qDebug() << "MapConfigManager: 加载设置完成";
    qDebug() << "  - 地图类型索引:" << m_mapTypeIndex;
    qDebug() << "  - 缩放级别:" << m_zoomLevel;
    qDebug() << "  - 地图中心:" << m_mapCenter.latitude() << "," << m_mapCenter.longitude();
    qDebug() << "  - 坐标转换:" << (m_coordinateConversionEnabled ? "启用" : "禁用");
}

void MapConfigManager::saveSettings()
{
    m_settings->beginGroup("MapSettings");
    
    m_settings->setValue("mapTypeIndex", m_mapTypeIndex);
    m_settings->setValue("zoomLevel", m_zoomLevel);
    m_settings->setValue("centerLatitude", m_mapCenter.latitude());
    m_settings->setValue("centerLongitude", m_mapCenter.longitude());
    m_settings->setValue("coordinateConversionEnabled", m_coordinateConversionEnabled);
    
    m_settings->endGroup();
    m_settings->sync(); // 立即写入文件
    
    qDebug() << "MapConfigManager: 保存设置完成";
}