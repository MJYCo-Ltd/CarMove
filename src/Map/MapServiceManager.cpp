#include "Map/MapServiceManager.h"

#include "Map/TianDiTu/TiandituGeocoder.h"
#include "Map/TianDiTu/TiandituRoutePlanner.h"

MapServiceManager::MapServiceManager(QObject* parent)
    : QObject(parent)
    , m_geocoder(new TiandituGeocoder(this))
    , m_routePlanner(new TiandituRoutePlanner(this))
{
}

MapServiceManager::~MapServiceManager() = default;
