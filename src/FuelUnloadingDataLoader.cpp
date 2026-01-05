#include "FuelUnloadingDataLoader.h"
#include <QFile>
#include <QJsonParseError>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>

FuelUnloadingDataLoader::FuelUnloadingDataLoader(QObject *parent)
    : QObject(parent)
    , m_isLoaded(false)
{
    // 自动加载本地数据文件
    QString dataFilePath = "data/fuel_unloading_records.json";
    loadFromFile(dataFilePath);
}

bool FuelUnloadingDataLoader::loadFromFile(const QString& filePath)
{
    clearError();
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QString("无法打开文件: %1").arg(filePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        setError(QString("JSON解析错误: %1").arg(parseError.errorString()));
        return false;
    }
    
    bool success = parseJsonData(doc);
    if (success) {
        emit dataLoaded(true, QString("成功从文件加载 %1 辆车的数据").arg(m_vehicles.size()));
        qDebug() << "FuelUnloadingDataLoader: 从文件加载数据成功:" << filePath;
    }
    
    return success;
}

bool FuelUnloadingDataLoader::loadFromResource(const QString& resourcePath)
{
    clearError();
    
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(QString("无法打开资源文件: %1").arg(resourcePath));
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        setError(QString("JSON解析错误: %1").arg(parseError.errorString()));
        return false;
    }
    
    bool success = parseJsonData(doc);
    if (success) {
        emit dataLoaded(true, QString("成功从资源加载 %1 辆车的数据").arg(m_vehicles.size()));
        qDebug() << "FuelUnloadingDataLoader: 从资源加载数据成功:" << resourcePath;
    }
    
    return success;
}

bool FuelUnloadingDataLoader::parseJsonData(const QJsonDocument& doc)
{
    if (!doc.isObject()) {
        setError("JSON根节点不是对象");
        return false;
    }
    
    QJsonObject root = doc.object();
    
    if (!root.contains("vehicles") || !root["vehicles"].isArray()) {
        setError("JSON中缺少vehicles数组");
        return false;
    }
    
    QJsonArray vehiclesArray = root["vehicles"].toArray();
    QVariantList newVehicles;
    
    for (const QJsonValue& vehicleValue : vehiclesArray) {
        if (!vehicleValue.isObject()) {
            continue;
        }
        
        QJsonObject vehicleObj = vehicleValue.toObject();
        QVariantMap vehicle;
        
        vehicle["plateNumber"] = vehicleObj["plateNumber"].toString();
        
        if (vehicleObj.contains("records") && vehicleObj["records"].isArray()) {
            QJsonArray recordsArray = vehicleObj["records"].toArray();
            QVariantList records;
            
            for (const QJsonValue& recordValue : recordsArray) {
                if (recordValue.isObject()) {
                    records.append(recordToVariant(recordValue.toObject()));
                }
            }
            
            vehicle["records"] = records;
        }
        
        newVehicles.append(vehicle);
    }
    
    if (newVehicles.isEmpty()) {
        setError("没有找到有效的车辆数据");
        return false;
    }
    
    m_vehicles = newVehicles;
    m_isLoaded = true;
    
    emit vehiclesChanged();
    emit isLoadedChanged();
    
    return true;
}

QVariantMap FuelUnloadingDataLoader::recordToVariant(const QJsonObject& record)
{
    QVariantMap result;
    
    result["date"] = record["date"].toString();
    result["time"] = record["time"].toString();
    result["fuelType"] = record["fuelType"].toString();
    result["amount"] = record["amount"].toDouble();
    result["longitude"] = record["longitude"].toDouble();
    result["latitude"] = record["latitude"].toDouble();
    result["correctedLongitude"] = record["correctedLongitude"].toDouble();
    result["correctedLatitude"] = record["correctedLatitude"].toDouble();
    
    return result;
}

QVariantList FuelUnloadingDataLoader::getAllRecords()
{
    QVariantList allRecords;
    
    for (const QVariant& vehicleVariant : m_vehicles) {
        QVariantMap vehicle = vehicleVariant.toMap();
        QString plateNumber = vehicle["plateNumber"].toString();
        QVariantList records = vehicle["records"].toList();
        
        for (const QVariant& recordVariant : records) {
            QVariantMap record = recordVariant.toMap();
            record["plateNumber"] = plateNumber; // 添加车牌号到记录中
            allRecords.append(record);
        }
    }
    
    return allRecords;
}

QVariantMap FuelUnloadingDataLoader::getStatistics()
{
    QVariantMap stats;
    
    int totalVehicles = m_vehicles.size();
    int totalRecords = 0;
    double totalGasoline = 0.0;
    double totalDiesel = 0.0;
    
    for (const QVariant& vehicleVariant : m_vehicles) {
        QVariantMap vehicle = vehicleVariant.toMap();
        QVariantList records = vehicle["records"].toList();
        
        totalRecords += records.size();
        
        for (const QVariant& recordVariant : records) {
            QVariantMap record = recordVariant.toMap();
            QString fuelType = record["fuelType"].toString();
            double amount = record["amount"].toDouble();
            
            if (fuelType == "汽油") {
                totalGasoline += amount;
            } else if (fuelType == "柴油") {
                totalDiesel += amount;
            }
        }
    }
    
    stats["totalVehicles"] = totalVehicles;
    stats["totalRecords"] = totalRecords;
    stats["totalGasoline"] = totalGasoline;
    stats["totalDiesel"] = totalDiesel;
    stats["totalFuel"] = totalGasoline + totalDiesel;
    
    return stats;
}

void FuelUnloadingDataLoader::clearData()
{
    m_vehicles.clear();
    m_isLoaded = false;
    clearError();
    
    emit vehiclesChanged();
    emit isLoadedChanged();
    
    qDebug() << "FuelUnloadingDataLoader: 数据已清除";
}

void FuelUnloadingDataLoader::setError(const QString& error)
{
    m_errorMessage = error;
    m_isLoaded = false;
    emit errorMessageChanged();
    emit isLoadedChanged();
    qWarning() << "FuelUnloadingDataLoader错误:" << error;
}

void FuelUnloadingDataLoader::clearError()
{
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged();
    }
}