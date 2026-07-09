#include "DataParsing/ExcelLoadUtils.h"

#include "Core/AppLogger.h"
#include "Core/ErrorHandler.h"

namespace ExcelLoadUtils {

bool appendParsedVehicleRecord(int row,
                               ExcelDataReader::VehicleRecord& record,
                               QList<ExcelDataReader::VehicleRecord>& records,
                               int& validRecords,
                               int& skippedRows,
                               QStringList& errorSummary)
{
    if (!record.isValid()) {
        ++skippedRows;
        if (errorSummary.size() < 10) {
            errorSummary.append(QStringLiteral("第%1行数据验证失败：车牌号=%2")
                                  .arg(row)
                                  .arg(record.plateNumber));
        }
        return false;
    }

    if (!record.isInChinaRange()) {
        AppLogger::warn(QStringLiteral("警告：车辆 %1 在第 %2 行的坐标可能不在中国境内: (%3, %4)")
                            .arg(record.plateNumber)
                            .arg(row)
                            .arg(record.latitude)
                            .arg(record.longitude));
    }

    if (record.speed > 300.0) {
        AppLogger::warn(QStringLiteral("警告：车辆 %1 在第 %2 行的速度异常高: %3 km/h")
                            .arg(record.plateNumber)
                            .arg(row)
                            .arg(record.speed));
    }

    records.append(record);
    ++validRecords;
    return true;
}

bool finalizeVehicleLoad(const QString& fileName,
                         int processedRows,
                         int validRecords,
                         int skippedRows,
                         const QStringList& errorSummary,
                         QString& errorMessage)
{
    if (validRecords == 0) {
        errorMessage = HANDLE_DATA_ERROR(
            fileName,
            QStringLiteral("文件中没有有效的车辆数据。处理了%1行，跳过了%2行无效数据。")
                .arg(processedRows)
                .arg(skippedRows));
        if (!errorSummary.isEmpty()) {
            errorMessage += QStringLiteral("\n\n错误示例：\n%1").arg(errorSummary.join('\n'));
        }
        return false;
    }

    if (skippedRows > processedRows * 0.1) {
        AppLogger::warn(QStringLiteral("警告：跳过了较多无效数据行 (%1/%2)，请检查数据质量")
                            .arg(skippedRows)
                            .arg(processedRows));
    }

    return true;
}

} // namespace ExcelLoadUtils
