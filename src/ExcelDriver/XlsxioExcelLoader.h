#ifndef XLSXIOEXCELLOADER_H
#define XLSXIOEXCELLOADER_H

#include "Domain/TrajectoryTypes.h"
#include <functional>

class XlsxioExcelLoader
{
public:
    using ProgressCallback = std::function<void(int percentage)>;

    static bool load(const QString& filePath,
                     QList<TrajectoryPoint>& records,
                     const ProgressCallback& onProgress,
                     QString& errorMessage);
};

#endif // XLSXIOEXCELLOADER_H
