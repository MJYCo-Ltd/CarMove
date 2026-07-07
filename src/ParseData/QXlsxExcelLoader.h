#ifndef QXLSXEXCELLOADER_H
#define QXLSXEXCELLOADER_H

#include "ParseData/ExcelDataReader.h"
#include <functional>

class QXlsxExcelLoader
{
public:
    using ProgressCallback = std::function<void(int percentage)>;

    static bool load(const QString& filePath,
                     QList<ExcelDataReader::VehicleRecord>& records,
                     const ProgressCallback& onProgress,
                     QString& errorMessage);
};

#endif // QXLSXEXCELLOADER_H
