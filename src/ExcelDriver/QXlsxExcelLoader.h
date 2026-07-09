#ifndef QXLSXEXCELLOADER_H
#define QXLSXEXCELLOADER_H

#include "Domain/TrajectoryTypes.h"
#include <functional>

class QXlsxExcelLoader
{
public:
    using ProgressCallback = std::function<void(int percentage)>;

    static bool load(const QString& filePath,
                     QList<TrajectoryPoint>& records,
                     const ProgressCallback& onProgress,
                     QString& errorMessage);
};

#endif // QXLSXEXCELLOADER_H
