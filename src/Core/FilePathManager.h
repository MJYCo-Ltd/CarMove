#ifndef FILEPATHMANAGER_H
#define FILEPATHMANAGER_H

#include <QObject>
#include <QString>

/**
 * @brief 路径管理器：统一本地路径规范化与截图文件路径。
 */
class FilePathManager : public QObject
{
    Q_OBJECT

public:
    explicit FilePathManager(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QString normalizeLocalPath(const QString& path) const;
    Q_INVOKABLE bool ensureScreenshotOutputDirectory(const QString& folderPath) const;
    Q_INVOKABLE QString screenshotFilePath(const QString& folderPath,
                                           const QString& plateNumber,
                                           const QString& startDateIso,
                                           const QString& endDateIso) const;
    Q_INVOKABLE bool screenshotFileExists(const QString& folderPath,
                                          const QString& plateNumber,
                                          const QString& startDateIso,
                                          const QString& endDateIso) const;
};

#endif // FILEPATHMANAGER_H
