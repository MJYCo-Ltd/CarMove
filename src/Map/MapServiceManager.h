#ifndef MAPSERVICEMANAGER_H
#define MAPSERVICEMANAGER_H

#include <QObject>

#include "Map/TianDiTu/TiandituGeocoder.h"
#include "Map/TianDiTu/TiandituRoutePlanner.h"

/**
 * @brief 地图服务管理器：统一对外提供地理编码与路线规划，隐藏天地图 API 实现。
 */
class MapServiceManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(TiandituGeocoder* geocoder READ geocoder CONSTANT)
    Q_PROPERTY(TiandituRoutePlanner* routePlanner READ routePlanner CONSTANT)

public:
    explicit MapServiceManager(QObject* parent = nullptr);
    ~MapServiceManager() override;

    TiandituGeocoder* geocoder() const { return m_geocoder; }
    TiandituRoutePlanner* routePlanner() const { return m_routePlanner; }

private:
    TiandituGeocoder* m_geocoder = nullptr;
    TiandituRoutePlanner* m_routePlanner = nullptr;
};

#endif // MAPSERVICEMANAGER_H
