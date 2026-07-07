#ifndef EXCELFILEPATH_H
#define EXCELFILEPATH_H

#include <QFile>
#include <QFileInfo>
#include <QString>

struct xlsxio_read_struct;
typedef struct xlsxio_read_struct* xlsxioreader;

namespace ExcelFilePath {

QString normalizeLocalFilePath(const QString& path);

bool openReadableFile(QFile& file, const QString& localFilePath);

xlsxioreader openXlsxioReader(const QString& localFilePath);

bool validateExcelFile(const QString& localPath, QFileInfo& fileInfo, QString& errorMessage);

} // namespace ExcelFilePath

#endif // EXCELFILEPATH_H
