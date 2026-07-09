#include "Core/FilePathManager.h"

#include "Core/LocalFilePath.h"

#include <QDir>
#include <QFile>

QString FilePathManager::normalizeLocalPath(const QString& path) const
{
    return LocalFilePath::normalizeLocalFilePath(path);
}

bool FilePathManager::ensureScreenshotOutputDirectory(const QString& folderPath) const
{
    const QString localPath = LocalFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.trimmed().isEmpty()) {
        return false;
    }
    return QDir().mkpath(localPath);
}

QString FilePathManager::screenshotFilePath(const QString& folderPath,
                                            const QString& plateNumber,
                                            const QString& startDateIso,
                                            const QString& endDateIso) const
{
    const QString localFolder = LocalFilePath::normalizeLocalFilePath(folderPath);
    const QString safePlate = LocalFilePath::sanitizePlateForFilename(plateNumber);
    const QString fileName =
        safePlate + QLatin1Char('_') + startDateIso + QLatin1Char('_') + endDateIso + QStringLiteral(".png");
    return QDir(localFolder).filePath(fileName);
}

bool FilePathManager::screenshotFileExists(const QString& folderPath,
                                           const QString& plateNumber,
                                           const QString& startDateIso,
                                           const QString& endDateIso) const
{
    return QFile::exists(screenshotFilePath(folderPath, plateNumber, startDateIso, endDateIso));
}
