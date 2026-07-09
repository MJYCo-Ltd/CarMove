#ifndef XLSXIOEXCELLOADER_H
#define XLSXIOEXCELLOADER_H

#include "DataParsing/ExcelDataReader.h"
#include <functional>

class XlsxioExcelLoader
{
public:
    using ProgressCallback = std::function<void(int percentage)>;

    static bool load(const QString& filePath,
                     QList<ExcelDataReader::VehicleRecord>& records,
                     const ProgressCallback& onProgress,
                     QString& errorMessage);
};

#endif // XLSXIOEXCELLOADER_H
