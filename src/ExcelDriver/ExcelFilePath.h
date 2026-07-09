#ifndef EXCELFILEPATH_H
#define EXCELFILEPATH_H

#include <QFileInfo>
#include <QString>

struct xlsxio_read_struct;
typedef struct xlsxio_read_struct* xlsxioreader;

namespace ExcelFilePath {

xlsxioreader openXlsxioReader(const QString& localFilePath);

bool validateExcelFile(const QString& localPath, QFileInfo& fileInfo, QString& errorMessage);

} // namespace ExcelFilePath

#endif // EXCELFILEPATH_H