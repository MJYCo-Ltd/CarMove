#include "DataManagement/TrajectoryImportManager.h"

#include "Core/ConfigManager.h"
#include "DataManagement/PostGisDataManager.h"

TrajectoryImportManager::TrajectoryImportManager(QObject* parent)
    : QObject(parent)
    , m_postGisData(new PostGisDataManager(this))
{
    connect(m_postGisData, &PostGisDataManager::importProgress,
            this, &TrajectoryImportManager::importProgress);
}

bool TrajectoryImportManager::importFolder(const QString& folderPath, QString* statusMessageOut)
{
    QString errorMessage;
    TrajectoryImportResult result;
    const PostGisDatabaseConfig config = ConfigManager::GetInstance()->postGisDatabaseConfig();
    const bool success = m_postGisData->importFolder(folderPath, config, errorMessage, &result);

    if (statusMessageOut != nullptr) {
        if (!success) {
            *statusMessageOut = errorMessage;
        } else {
            QString statusMessage =
                QStringLiteral("导入完成：导入 %1/%2 个文件，跳过 %3 个，新增 %4 个轨迹点")
                    .arg(result.importedFiles)
                    .arg(result.totalFiles)
                    .arg(result.skippedFiles)
                    .arg(result.importedPoints);
            if (result.failedFiles > 0) {
                statusMessage += QStringLiteral("；失败 %1 个").arg(result.failedFiles);
                if (!result.errorSamples.isEmpty()) {
                    statusMessage += QStringLiteral("（%1").arg(result.errorSamples.first());
                    if (result.errorSamples.size() > 1) {
                        statusMessage += QStringLiteral(" 等");
                    }
                    statusMessage += QStringLiteral("）");
                }
            }
            *statusMessageOut = statusMessage;
        }
    }

    return success;
}
