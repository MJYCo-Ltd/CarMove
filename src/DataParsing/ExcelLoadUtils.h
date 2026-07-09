#ifndef EXCELLOADUTILS_H
#define EXCELLOADUTILS_H

#include "Domain/TrajectoryTypes.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace ExcelLoadUtils {

bool appendParsedVehicleRecord(int row,
                               TrajectoryPoint& record,
                               QList<TrajectoryPoint>& records,
                               int& validRecords,
                               int& skippedRows,
                               QStringList& errorSummary);

bool finalizeVehicleLoad(const QString& fileName,
                         int processedRows,
                         int validRecords,
                         int skippedRows,
                         const QStringList& errorSummary,
                         QString& errorMessage);

} // namespace ExcelLoadUtils

#endif // EXCELLOADUTILS_H
