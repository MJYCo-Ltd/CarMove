#ifndef COORDINATECONVERTER_H
#define COORDINATECONVERTER_H

#include <QObject>
#include <QGeoCoordinate>
#include <QList>

class CoordinateConverter : public QObject
{
    Q_OBJECT
    
public:
    enum CoordinateSystem {
        WGS84,      // 标准GPS坐标系
        GCJ02       // 中国火星坐标系（测绘局加密）
    };
    
    explicit CoordinateConverter(QObject *parent = nullptr);
    
    // WGS84转GCJ02（GPS坐标转火星坐标）
    static QGeoCoordinate wgs84ToGcj02(const QGeoCoordinate& wgs84Coord);
    
    // GCJ02转WGS84（火星坐标转GPS坐标）
    static QGeoCoordinate gcj02ToWgs84(const QGeoCoordinate& gcj02Coord);
    
    // 批量转换轨迹点
    static QList<QGeoCoordinate> convertTrajectory(const QList<QGeoCoordinate>& coords, 
                                                  CoordinateSystem from, 
                                                  CoordinateSystem to);
    
    // 判断坐标是否在中国境内（需要转换）
    static bool isInChina(const QGeoCoordinate& coord);
    
    // 自动检测并建议坐标系转换
    static CoordinateSystem detectCoordinateSystem(const QList<QGeoCoordinate>& coords);
    
private:
    // 坐标转换算法常量
    static constexpr double PI = 3.1415926535897932384626;
    static constexpr double A = 6378245.0;  // 长半轴
    static constexpr double EE = 0.00669342162296594323;  // 偏心率平方
    
    // 转换算法辅助函数
    static double transformLat(double lng, double lat);
    static double transformLng(double lng, double lat);
    static bool outOfChina(double lng, double lat);
};

#endif // COORDINATECONVERTER_H