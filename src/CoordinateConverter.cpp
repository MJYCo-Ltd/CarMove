#include "CoordinateConverter.h"
#include "ErrorHandler.h"
#include <QtMath>

CoordinateConverter::CoordinateConverter(QObject *parent)
    : QObject(parent)
{
}

QGeoCoordinate CoordinateConverter::wgs84ToGcj02(const QGeoCoordinate& wgs84Coord)
{
    if (!wgs84Coord.isValid()) {
        qWarning() << "Invalid WGS84 coordinate provided for conversion";
        return QGeoCoordinate();
    }
    
    double lng = wgs84Coord.longitude();
    double lat = wgs84Coord.latitude();
    
    // Validate coordinate ranges
    if (lng < -180.0 || lng > 180.0 || lat < -90.0 || lat > 90.0) {
        qWarning() << "Coordinate out of valid range - Lng:" << lng << "Lat:" << lat;
        return QGeoCoordinate();
    }
    
    try {
        // 如果不在中国境内，直接返回原坐标
        if (outOfChina(lng, lat)) {
            return wgs84Coord;
        }
        
        double dLat = transformLat(lng - 105.0, lat - 35.0);
        double dLng = transformLng(lng - 105.0, lat - 35.0);
        
        double radLat = lat / 180.0 * PI;
        double magic = qSin(radLat);
        magic = 1 - EE * magic * magic;
        double sqrtMagic = qSqrt(magic);
        dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * PI);
        dLng = (dLng * 180.0) / (A / sqrtMagic * qCos(radLat) * PI);
        
        double mgLat = lat + dLat;
        double mgLng = lng + dLng;
        
        // Validate result
        if (mgLng < -180.0 || mgLng > 180.0 || mgLat < -90.0 || mgLat > 90.0) {
            qWarning() << "Coordinate conversion resulted in invalid coordinates - Original:" 
                       << lng << lat << "Converted:" << mgLng << mgLat;
            return wgs84Coord; // Return original if conversion failed
        }
        
        return QGeoCoordinate(mgLat, mgLng, wgs84Coord.altitude());
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during WGS84 to GCJ02 conversion:" << e.what();
        return wgs84Coord;
    } catch (...) {
        qWarning() << "Unknown exception during WGS84 to GCJ02 conversion";
        return wgs84Coord;
    }
}

QGeoCoordinate CoordinateConverter::gcj02ToWgs84(const QGeoCoordinate& gcj02Coord)
{
    if (!gcj02Coord.isValid()) {
        qWarning() << "Invalid GCJ02 coordinate provided for conversion";
        return QGeoCoordinate();
    }
    
    double lng = gcj02Coord.longitude();
    double lat = gcj02Coord.latitude();
    
    // Validate coordinate ranges
    if (lng < -180.0 || lng > 180.0 || lat < -90.0 || lat > 90.0) {
        qWarning() << "Coordinate out of valid range - Lng:" << lng << "Lat:" << lat;
        return QGeoCoordinate();
    }
    
    try {
        // 如果不在中国境内，直接返回原坐标
        if (outOfChina(lng, lat)) {
            return gcj02Coord;
        }
        
        double dLat = transformLat(lng - 105.0, lat - 35.0);
        double dLng = transformLng(lng - 105.0, lat - 35.0);
        
        double radLat = lat / 180.0 * PI;
        double magic = qSin(radLat);
        magic = 1 - EE * magic * magic;
        double sqrtMagic = qSqrt(magic);
        dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * PI);
        dLng = (dLng * 180.0) / (A / sqrtMagic * qCos(radLat) * PI);
        
        double mgLat = lat - dLat;
        double mgLng = lng - dLng;
        
        // Validate result
        if (mgLng < -180.0 || mgLng > 180.0 || mgLat < -90.0 || mgLat > 90.0) {
            qWarning() << "Coordinate conversion resulted in invalid coordinates - Original:" 
                       << lng << lat << "Converted:" << mgLng << mgLat;
            return gcj02Coord; // Return original if conversion failed
        }
        
        return QGeoCoordinate(mgLat, mgLng, gcj02Coord.altitude());
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during GCJ02 to WGS84 conversion:" << e.what();
        return gcj02Coord;
    } catch (...) {
        qWarning() << "Unknown exception during GCJ02 to WGS84 conversion";
        return gcj02Coord;
    }
}

QList<QGeoCoordinate> CoordinateConverter::convertTrajectory(const QList<QGeoCoordinate>& coords, 
                                                           CoordinateSystem from, 
                                                           CoordinateSystem to)
{
    QList<QGeoCoordinate> result;
    
    // 如果源坐标系和目标坐标系相同，直接返回原坐标
    if (from == to) {
        return coords;
    }
    
    if (coords.isEmpty()) {
        return result;
    }
    
    result.reserve(coords.size());
    int conversionErrors = 0;
    int validConversions = 0;
    
    try {
        for (int i = 0; i < coords.size(); ++i) {
            const QGeoCoordinate& coord = coords[i];
            
            if (!coord.isValid()) {
                result.append(coord); // 保持无效坐标不变
                continue;
            }
            
            QGeoCoordinate convertedCoord;
            if (from == WGS84 && to == GCJ02) {
                convertedCoord = wgs84ToGcj02(coord);
            } else if (from == GCJ02 && to == WGS84) {
                convertedCoord = gcj02ToWgs84(coord);
            } else {
                convertedCoord = coord; // 未知转换类型，保持原坐标
                qWarning() << "Unknown coordinate conversion type requested";
            }
            
            // Check if conversion was successful
            if (convertedCoord.isValid()) {
                validConversions++;
            } else {
                conversionErrors++;
                convertedCoord = coord; // Use original if conversion failed
            }
            
            result.append(convertedCoord);
        }
        
        // Log conversion statistics
        if (conversionErrors > 0) {
            qWarning() << QString("Coordinate conversion completed with %1 errors out of %2 coordinates")
                         .arg(conversionErrors).arg(coords.size());
        } else {
        }
        
        // If too many conversion errors, warn user
        if (conversionErrors > coords.size() * 0.1) { // More than 10% failed
            qWarning() << "High number of coordinate conversion failures detected";
        }
        
    } catch (const std::exception& e) {
        qWarning() << "Exception during trajectory conversion:" << e.what();
        return coords; // Return original coordinates on error
    } catch (...) {
        qWarning() << "Unknown exception during trajectory conversion";
        return coords; // Return original coordinates on error
    }
    
    return result;
}

bool CoordinateConverter::isInChina(const QGeoCoordinate& coord)
{
    if (!coord.isValid()) {
        return false;
    }
    
    return !outOfChina(coord.longitude(), coord.latitude());
}

CoordinateConverter::CoordinateSystem CoordinateConverter::detectCoordinateSystem(const QList<QGeoCoordinate>& coords)
{
    if (coords.isEmpty()) {
        return WGS84; // 默认返回WGS84
    }
    
    // 简单的启发式检测：
    // 如果大部分坐标在中国境内，且坐标看起来像是经过偏移的（GCJ02特征），
    // 则可能是GCJ02坐标系
    int chinaCount = 0;
    int totalValid = 0;
    
    for (const QGeoCoordinate& coord : coords) {
        if (coord.isValid()) {
            totalValid++;
            if (isInChina(coord)) {
                chinaCount++;
            }
        }
    }
    
    // 如果超过80%的坐标在中国境内，可能需要坐标转换
    // 这里返回WGS84作为默认值，实际应用中可能需要更复杂的检测逻辑
    if (totalValid > 0 && (double)chinaCount / totalValid > 0.8) {
        return WGS84; // 假设输入数据是WGS84，需要转换为GCJ02用于中国地图显示
    }
    
    return WGS84;
}

double CoordinateConverter::transformLat(double lng, double lat)
{
    double ret = -100.0 + 2.0 * lng + 3.0 * lat + 0.2 * lat * lat + 
                 0.1 * lng * lat + 0.2 * qSqrt(qAbs(lng));
    ret += (20.0 * qSin(6.0 * lng * PI) + 20.0 * qSin(2.0 * lng * PI)) * 2.0 / 3.0;
    ret += (20.0 * qSin(lat * PI) + 40.0 * qSin(lat / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * qSin(lat / 12.0 * PI) + 320 * qSin(lat * PI / 30.0)) * 2.0 / 3.0;
    return ret;
}

double CoordinateConverter::transformLng(double lng, double lat)
{
    double ret = 300.0 + lng + 2.0 * lat + 0.1 * lng * lng + 
                 0.1 * lng * lat + 0.1 * qSqrt(qAbs(lng));
    ret += (20.0 * qSin(6.0 * lng * PI) + 20.0 * qSin(2.0 * lng * PI)) * 2.0 / 3.0;
    ret += (20.0 * qSin(lng * PI) + 40.0 * qSin(lng / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * qSin(lng / 12.0 * PI) + 300.0 * qSin(lng / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
}

bool CoordinateConverter::outOfChina(double lng, double lat)
{
    // 中国境内经纬度范围的粗略判断
    // 经度范围：73.66 - 135.05
    // 纬度范围：3.86 - 53.55
    if (lng < 72.004 || lng > 137.8347) {
        return true;
    }
    if (lat < 0.8293 || lat > 55.8271) {
        return true;
    }
    return false;
}