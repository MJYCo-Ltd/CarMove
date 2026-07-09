#include "Core/LocalFilePath.h"

#include <QDir>
#include <QRegularExpression>
#include <QUrl>

namespace LocalFilePath {

QString normalizeLocalFilePath(const QString& path)
{
    if (path.isEmpty()) {
        return path;
    }

    const QUrl url(path);
    if (url.isValid() && url.scheme().compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
        const QString localPath = url.toLocalFile();
        if (!localPath.isEmpty()) {
            return QDir::toNativeSeparators(localPath);
        }
    }

    return QDir::toNativeSeparators(path);
}

bool openReadableFile(QFile& file, const QString& localFilePath)
{
    file.setFileName(localFilePath);
    return file.open(QIODevice::ReadOnly);
}

QString sanitizeFileComponent(const QString& name)
{
    QString sanitized = name.trimmed();
    sanitized.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    return sanitized;
}

QString sanitizePlateForFilename(const QString& plateNumber)
{
    QString safePlate = plateNumber.trimmed();
    for (const QChar ch : QStringLiteral("\\/:*?\"<>|")) {
        safePlate.replace(ch, QLatin1Char('_'));
    }
    return safePlate;
}

} // namespace LocalFilePath
