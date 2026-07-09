#include "ExcelDriver/XlsxioExcelLoader.h"
#include "DataParsing/ExcelRowParser.h"
#include "ExcelDriver/ExcelFilePath.h"
#include "DataParsing/ExcelLoadUtils.h"
#include "Core/ConfigManager.h"
#include "Core/ErrorHandler.h"
#include "Core/AppLogger.h"

#include <QCoreApplication>
#include <QFileInfo>

extern "C" {
#include "xlsxio_read.h"
}

namespace {

QHash<int, QVariant> readRowCells(xlsxioreadersheet sheet)
{
    QHash<int, QVariant> cells;
    int columnIndex = 1;
    XLSXIOCHAR* value = nullptr;

    while (xlsxioread_sheet_next_cell_string(sheet, &value)) {
        cells.insert(columnIndex, QString::fromUtf8(value));
        xlsxioread_free(value);
        value = nullptr;
        ++columnIndex;
    }

    return cells;
}

void discardRowCells(xlsxioreadersheet sheet)
{
    XLSXIOCHAR* value = nullptr;
    while (xlsxioread_sheet_next_cell_string(sheet, &value)) {
        xlsxioread_free(value);
        value = nullptr;
    }
}

} // namespace

bool XlsxioExcelLoader::load(const QString& filePath,
                             QList<ExcelDataReader::VehicleRecord>& records,
                             const ProgressCallback& onProgress,
                             QString& errorMessage)
{
    records.clear();
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(filePath);
    const QFileInfo fileInfo(localPath);
    const int dataStartRow = ConfigManager::GetInstance()->getExcelDataStartRow();

    xlsxioreader reader = ExcelFilePath::openXlsxioReader(localPath);
    if (!reader) {
        errorMessage = HANDLE_FILE_ERROR(localPath, QStringLiteral("打开Excel文件"));
        return false;
    }

    xlsxioreadersheet sheet =
        xlsxioread_sheet_open(reader, nullptr, XLSXIOREAD_SKIP_NONE);
    if (!sheet) {
        xlsxioread_close(reader);
        errorMessage = HANDLE_DATA_ERROR(fileInfo.fileName(), QStringLiteral("Excel文件中没有找到工作表"));
        return false;
    }

    AppLogger::info(QStringLiteral("使用 xlsxio 流式读取大文件: %1").arg(localPath));

    int processedRows = 0;
    int validRecords = 0;
    int skippedRows = 0;
    QStringList errorSummary;

    if (onProgress) {
        onProgress(0);
    }

    while (xlsxioread_sheet_next_row(sheet)) {
        const size_t rowIndex = xlsxioread_sheet_last_row_index(sheet);

        if (static_cast<int>(rowIndex) < dataStartRow) {
            discardRowCells(sheet);
            continue;
        }

        const QHash<int, QVariant> cells = readRowCells(sheet);

        ExcelDataReader::VehicleRecord record;
        QString rowError;
        if (ExcelRowParser::parseRow(cells, record, rowError)) {
            ExcelLoadUtils::appendParsedVehicleRecord(static_cast<int>(rowIndex),
                                                      record,
                                                      records,
                                                      validRecords,
                                                      skippedRows,
                                                      errorSummary);
        } else {
            ++skippedRows;
            QString parseError = QStringLiteral("第%1行数据解析失败").arg(rowIndex);
            if (!rowError.isEmpty()) {
                parseError += QStringLiteral("：%1").arg(rowError);
            }
            if (errorSummary.size() < 10) {
                errorSummary.append(parseError);
            }
        }

        ++processedRows;
        if (onProgress && processedRows % 500 == 0) {
            onProgress(qMin(99, processedRows / 100));
            QCoreApplication::processEvents();
        }
    }

    xlsxioread_sheet_close(sheet);
    xlsxioread_close(reader);

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
