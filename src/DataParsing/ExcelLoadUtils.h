#ifndef EXCELLOADUTILS_H
#define EXCELLOADUTILS_H

#include "DataParsing/ExcelDataReader.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace ExcelLoadUtils {

bool appendParsedVehicleRecord(int row,
                               ExcelDataReader::VehicleRecord& record,
                               QList<ExcelDataReader::VehicleRecord>& records,
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
