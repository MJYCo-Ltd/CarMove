#include "ExcelDriver/ExcelFilePath.h"

#include "Core/ErrorHandler.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QUrl>

extern "C" {
#include "xlsxio_read.h"
}

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace ExcelFilePath {

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

xlsxioreader openXlsxioReader(const QString& localFilePath)
{
#ifdef Q_OS_WIN
    const int fd = _wopen(reinterpret_cast<const wchar_t*>(localFilePath.utf16()),
                          _O_RDONLY | _O_BINARY);
    if (fd < 0) {
        return nullptr;
    }

    xlsxioreader reader = xlsxioread_open_filehandle(fd);
    if (!reader) {
        _close(fd);
    }
    return reader;
#else
    return xlsxioread_open(QFile::encodeName(localFilePath).constData());
#endif
}

bool validateExcelFile(const QString& localPath, QFileInfo& fileInfo, QString& errorMessage)
{
    fileInfo.setFile(localPath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        errorMessage = HANDLE_FILE_ERROR(localPath, QStringLiteral("读取"));
        return false;
    }

    const QString suffix = fileInfo.suffix().toLower();
    if (suffix != QLatin1String("xlsx") && suffix != QLatin1String("xls")) {
        errorMessage = HANDLE_DATA_ERROR(
            fileInfo.fileName(),
            QStringLiteral("不支持的文件格式: %1。支持的格式：.xlsx, .xls").arg(suffix));
        return false;
    }

    return true;
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

QString displayFolderUrl(const QString& folderUrl)
{
    if (folderUrl.isEmpty()) {
        return QStringLiteral("未选择");
    }

    const QString path = folderUrl;
    if (path.startsWith(QStringLiteral("file:///"))) {
        return QUrl(path).toLocalFile();
    }
    return path;
}

} // namespace ExcelFilePath
