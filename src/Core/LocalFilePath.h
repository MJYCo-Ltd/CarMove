#ifndef LOCALFILEPATH_H
#define LOCALFILEPATH_H

#include <QFile>
#include <QString>

namespace LocalFilePath {

QString normalizeLocalFilePath(const QString& path);

QString sanitizeFileComponent(const QString& name);

QString sanitizePlateForFilename(const QString& plateNumber);

bool openReadableFile(QFile& file, const QString& localFilePath);

} // namespace LocalFilePath

#endif // LOCALFILEPATH_H
