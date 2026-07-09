#include "ExcelDriver/QXlsxExcelLoader.h"
#include "ExcelDriver/ExcelFilePath.h"
#include "Core/LocalFilePath.h"
#include "DataParsing/ExcelLoadUtils.h"
#include "DataParsing/ExcelRowParser.h"
#include "Core/ConfigManager.h"
#include "Core/ErrorHandler.h"
#include "Core/AppLogger.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>

#include "xlsxdocument.h"

QXLSX_USE_NAMESPACE

bool QXlsxExcelLoader::load(const QString& filePath,
                            QList<TrajectoryPoint>& records,
                            const ProgressCallback& onProgress,
                            QString& errorMessage)
{
    records.clear();
    const QFileInfo fileInfo(filePath);

    QFile file;
    if (!LocalFilePath::openReadableFile(file, filePath)) {
        errorMessage = HANDLE_FILE_ERROR(filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    Document xlsx(&file);
    if (!xlsx.load()) {
        errorMessage = HANDLE_DATA_ERROR(fileInfo.fileName(),
                                         QStringLiteral("QXlsx 无法解析该 Excel 文件"));
        return false;
    }

    Worksheet* worksheet = xlsx.currentWorksheet();
    if (!worksheet) {
        errorMessage = HANDLE_DATA_ERROR(fileInfo.fileName(), QStringLiteral("Excel文件中没有找到工作表"));
        return false;
    }

    const CellRange range = worksheet->dimension();
    const int dataStartRow = ConfigManager::GetInstance()->getExcelDataStartRow();
    if (range.rowCount() < dataStartRow) {
        errorMessage = HANDLE_DATA_ERROR(
            fileInfo.fileName(),
            QStringLiteral("Excel文件行数不足。数据起始行为%1，但文件只有%2行")
                .arg(dataStartRow)
                .arg(range.rowCount()));
        return false;
    }

    const int totalCells = range.rowCount() * range.columnCount();
    if (totalCells > 1000000) {
        AppLogger::warn(QStringLiteral("Large dataset detected: %1 cells. This may take some time to process.")
                            .arg(totalCells));
    }

    const int totalRows = range.rowCount();
    int processedRows = 0;
    int validRecords = 0;
    int skippedRows = 0;
    QStringList errorSummary;

    if (onProgress) {
        onProgress(0);
    }

    for (int row = dataStartRow; row <= totalRows; ++row) {
        QHash<int, QVariant> cells;
        for (const auto& mapping : ConfigManager::GetInstance()->getExcelFieldMappings()) {
            if (mapping.isMapped()) {
                cells.insert(mapping.columnIndex, xlsx.read(row, mapping.columnIndex));
            }
        }

        TrajectoryPoint record;
        QString rowError;
        if (ExcelRowParser::parseRow(cells, record, rowError)) {
            ExcelLoadUtils::appendParsedVehicleRecord(row,
                                                      record,
                                                      records,
                                                      validRecords,
                                                      skippedRows,
                                                      errorSummary);
        } else {
            ++skippedRows;
            QString parseError = QStringLiteral("第%1行数据解析失败").arg(row);
            if (!rowError.isEmpty()) {
                parseError += QStringLiteral("：%1").arg(rowError);
            }
            if (errorSummary.size() < 10) {
                errorSummary.append(parseError);
            }
        }

        ++processedRows;
        if (onProgress && (processedRows % 100 == 0 || row == totalRows)) {
            const int progress =
                (processedRows * 100) / (totalRows - dataStartRow + 1);
            onProgress(progress);

            if (processedRows % 1000 == 0) {
                QCoreApplication::processEvents();
            }
        }
    }

    if (!ExcelLoadUtils::finalizeVehicleLoad(fileInfo.fileName(),
                                             processedRows,
                                             validRecords,
                                             skippedRows,
                                             errorSummary,
                                             errorMessage)) {
        return false;
    }

    if (onProgress) {
        onProgress(100);
    }

    return true;
}
