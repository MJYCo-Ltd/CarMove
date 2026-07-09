#ifndef TRAJECTORYIMPORTMANAGER_H
#define TRAJECTORYIMPORTMANAGER_H

#include "DataManagement/PostGisDataManager.h"

#include <QObject>
#include <QString>

/**
 * @brief 轨迹导入管理器：统一将本地 Excel 轨迹文件夹导入 PostGIS。
 */
class TrajectoryImportManager : public QObject
{
    Q_OBJECT

public:
    explicit TrajectoryImportManager(QObject* parent = nullptr);

    Q_INVOKABLE bool importFolder(const QString& folderPath, QString* statusMessageOut = nullptr);

signals:
    void importProgress(int percentage);

private:
    PostGisDataManager* m_postGisData = nullptr;
};

#endif // TRAJECTORYIMPORTMANAGER_H
