#ifndef FOLDERSCANNER_H
#define FOLDERSCANNER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>

class FolderScanner : public QObject
{
    Q_OBJECT
    
public:
    struct VehicleInfo {
        QString plateNumber;
        QStringList filePaths;        // 改为文件路径列表，支持多个文件
        QDateTime firstTimestamp;
        QDateTime lastTimestamp;
        int recordCount;
    };
    
    explicit FolderScanner(QObject *parent = nullptr);
    
    void scanFolder(const QString& folderPath);
    QList<VehicleInfo> getVehicleList() const;
    
signals:
    void scanCompleted(const QList<VehicleInfo>& vehicles);
    void scanProgress(int percentage);
    void scanError(const QString& error);
    
private:
    QList<VehicleInfo> m_vehicleList;
};

#endif // FOLDERSCANNER_H