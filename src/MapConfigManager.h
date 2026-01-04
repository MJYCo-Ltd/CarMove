#ifndef MAPCONFIGMANAGER_H
#define MAPCONFIGMANAGER_H

#include <QObject>
#include <QSettings>
#include <QGeoCoordinate>
#include <QString>
#include <QQmlEngine>

class MapConfigManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    
    Q_PROPERTY(int mapTypeIndex READ mapTypeIndex WRITE setMapTypeIndex NOTIFY mapTypeIndexChanged)
    Q_PROPERTY(double zoomLevel READ zoomLevel WRITE setZoomLevel NOTIFY zoomLevelChanged)
    Q_PROPERTY(QGeoCoordinate mapCenter READ mapCenter WRITE setMapCenter NOTIFY mapCenterChanged)
    Q_PROPERTY(bool coordinateConversionEnabled READ coordinateConversionEnabled WRITE setCoordinateConversionEnabled NOTIFY coordinateConversionEnabledChanged)
    
public:
    explicit MapConfigManager(QObject *parent = nullptr);
    ~MapConfigManager();
    
    // Property getters
    int mapTypeIndex() const { return m_mapTypeIndex; }
    double zoomLevel() const { return m_zoomLevel; }
    QGeoCoordinate mapCenter() const { return m_mapCenter; }
    bool coordinateConversionEnabled() const { return m_coordinateConversionEnabled; }
    
    // Property setters
    void setMapTypeIndex(int index);
    void setZoomLevel(double level);
    void setMapCenter(const QGeoCoordinate& center);
    void setCoordinateConversionEnabled(bool enabled);
    
    // Invokable methods for QML
    Q_INVOKABLE void saveMapState();
    Q_INVOKABLE void loadMapState();
    Q_INVOKABLE void resetToDefaults();
    
signals:
    void mapTypeIndexChanged();
    void zoomLevelChanged();
    void mapCenterChanged();
    void coordinateConversionEnabledChanged();
    void mapStateLoaded();
    
private:
    void loadSettings();
    void saveSettings();
    
    // Configuration properties
    int m_mapTypeIndex;
    double m_zoomLevel;
    QGeoCoordinate m_mapCenter;
    bool m_coordinateConversionEnabled;
    
    // Settings instance
    QSettings* m_settings;
    
    // Default values
    static const int DEFAULT_MAP_TYPE_INDEX = 0;
    static inline const double DEFAULT_ZOOM_LEVEL = 12.0;
    static inline const double DEFAULT_LATITUDE = 39.9;
    static inline const double DEFAULT_LONGITUDE = 116.4;
    static const bool DEFAULT_COORDINATE_CONVERSION = false;
};

#endif // MAPCONFIGMANAGER_H
