#include "ExcelDriver/ExcelParserPreviewInternal.h"

#include "ExcelDriver/ExcelBackend.h"
#include "ExcelDriver/ExcelCellFormatter.h"
#include "ExcelDriver/ExcelFilePath.h"
#include "ExcelDriver/ExcelPreviewTypes.h"
#include "ExcelDriver/ExcelSheetGridUtils.h"
#include "ExcelDriver/OoxmlSaxExcelLoader.h"
#include "Core/ErrorHandler.h"
#include "Core/LocalFilePath.h"
#include "Core/AppLogger.h"

#include <QFileInfo>
#include <QMap>

#include <QFile>

#include "xlsxdocument.h"
#include "xlsxworkbook.h"

QXLSX_USE_NAMESPACE

extern "C" {
#include "xlsxio_read.h"
}

namespace ExcelParserPreviewInternal {

namespace {

struct XlsxioProcessContext {
    ExcelPreviewLimits limits;
    QMap<int, QMap<int, QString>> cells;
    bool truncated = false;
};

int xlsxioPreviewCellCallback(size_t row, size_t col, const XLSXIOCHAR* value, void* callbackdata)
{
    auto* context = static_cast<XlsxioProcessContext*>(callbackdata);
    const int rowIndex = static_cast<int>(row);
    const int columnIndex = static_cast<int>(col);

    if (rowIndex > context->limits.maxRows) {
        context->truncated = true;
        return 1;
    }
    if (columnIndex > context->limits.maxColumns) {
        context->truncated = true;
        return 0;
    }

    if (!value || value[0] == '\0') {
        return 0;
    }

    const QString text = QString::fromUtf8(value).trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    context->cells[rowIndex][columnIndex] = text;
    return 0;
}

bool inspectWithQXlsx(const QString& filePath, ExcelWorkbookInfo& info, QString& errorMessage)
{
    QFile file;
    if (!LocalFilePath::openReadableFile(file, filePath)) {
        errorMessage = HANDLE_FILE_ERROR(filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    Document xlsx(&file);
    if (!xlsx.load()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("QXlsx 无法解析该 Excel 文件"));
        return false;
    }

    info.sheetNames = xlsx.sheetNames();
    if (info.sheetNames.isEmpty()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("Excel文件中没有找到工作表"));
        return false;
    }
    return true;
}

bool inspectWithXlsxio(const QString& filePath, ExcelWorkbookInfo& info, QString& errorMessage)
{
    xlsxioreader reader = ExcelFilePath::openXlsxioReader(filePath);
    if (!reader) {
        errorMessage = HANDLE_FILE_ERROR(filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    xlsxioreadersheetlist sheetList = xlsxioread_sheetlist_open(reader);
    if (!sheetList) {
        xlsxioread_close(reader);
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("Excel文件中没有找到工作表"));
        return false;
    }

    info.sheetNames.clear();
    const XLSXIOCHAR* sheetName = nullptr;
    while ((sheetName = xlsxioread_sheetlist_next(sheetList)) != nullptr) {
        info.sheetNames.append(QString::fromUtf8(sheetName));
    }

    xlsxioread_sheetlist_close(sheetList);
    xlsxioread_close(reader);

    if (info.sheetNames.isEmpty()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("Excel文件中没有找到工作表"));
        return false;
    }
    return true;
}

bool loadSheetWithQXlsx(const ExcelWorkbookInfo& info,
                        int sheetIndex,
                        ExcelSheetPreview& sheet,
                        QString& errorMessage)
{
    if (sheetIndex < 0 || sheetIndex >= info.sheetNames.size()) {
        errorMessage = QStringLiteral("工作表索引无效");
        return false;
    }

    QFile file;
    if (!LocalFilePath::openReadableFile(file, info.filePath)) {
        errorMessage = HANDLE_FILE_ERROR(info.filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    Document xlsx(&file);
    if (!xlsx.load()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(info.filePath).fileName(),
                                         QStringLiteral("QXlsx 无法解析该 Excel 文件"));
        return false;
    }

    const QString sheetName = info.sheetNames.at(sheetIndex);
    if (!xlsx.selectSheet(sheetName)) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(info.filePath).fileName(),
                                         QStringLiteral("无法打开工作表 %1").arg(sheetName));
        return false;
    }

    sheet.name = sheetName;
    sheet.grid.clear();
    sheet.columnCount = 0;
    sheet.statusMessage.clear();

    Worksheet* worksheet = xlsx.currentWorksheet();
    if (!worksheet) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    const CellRange range = worksheet->dimension();
    if (range.rowCount() <= 0 || range.columnCount() <= 0) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    const int totalRows = range.rowCount();
    const int rowsToLoad = qMin(totalRows, info.limits.maxRows);
    sheet.columnCount = qMin(range.columnCount(), info.limits.maxColumns);
    const bool truncated = totalRows > rowsToLoad || range.columnCount() > sheet.columnCount;

    const bool isDate1904 = xlsx.workbook() && xlsx.workbook()->isDate1904();

    sheet.grid.reserve(rowsToLoad);
    sheet.originalRowNumbers.reserve(rowsToLoad);
    for (int row = 1; row <= rowsToLoad; ++row) {
        QVector<QString> rowData;
        rowData.reserve(sheet.columnCount);
        for (int col = 1; col <= sheet.columnCount; ++col) {
            rowData.append(ExcelCellFormatter::formatQXlsxCell(worksheet->cellAt(row, col),
                                                               isDate1904));
        }
        sheet.grid.append(std::move(rowData));
        sheet.originalRowNumbers.append(row);
    }

    ExcelSheetGridUtils::compressGridToUsedColumns(sheet.grid, sheet.columnCount);

    if (sheet.grid.isEmpty()) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    sheet.statusMessage = ExcelSheetGridUtils::formatSheetStatus(sheet.grid.size(),
                                                                 sheet.columnCount,
                                                                 truncated,
                                                                 info.limits);
    return true;
}

bool loadSheetWithXlsxio(const ExcelWorkbookInfo& info,
                         int sheetIndex,
                         ExcelSheetPreview& sheet,
                         QString& errorMessage)
{
    if (sheetIndex < 0 || sheetIndex >= info.sheetNames.size()) {
        errorMessage = QStringLiteral("工作表索引无效");
        return false;
    }

    xlsxioreader reader = ExcelFilePath::openXlsxioReader(info.filePath);
    if (!reader) {
        errorMessage = HANDLE_FILE_ERROR(info.filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    sheet.name = info.sheetNames.at(sheetIndex);
    sheet.grid.clear();
    sheet.columnCount = 0;
    sheet.statusMessage.clear();

    XlsxioProcessContext context;
    context.limits = info.limits;

    const QByteArray sheetNameUtf8 = sheet.name.toUtf8();
    const unsigned int flags = XLSXIOREAD_SKIP_EMPTY_CELLS | XLSXIOREAD_SKIP_EXTRA_CELLS;
    const int processResult = xlsxioread_process(reader,
                                                 sheetNameUtf8.constData(),
                                                 flags,
                                                 xlsxioPreviewCellCallback,
                                                 nullptr,
                                                 &context);

    xlsxioread_close(reader);

    if (processResult != 0 && context.cells.isEmpty()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(info.filePath).fileName(),
                                         QStringLiteral("无法读取工作表 %1").arg(sheet.name));
        return false;
    }

    if (context.cells.isEmpty()) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    ExcelSheetGridUtils::buildGridFromSparseCells(context.cells, sheet);

    if (sheet.grid.isEmpty()) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    sheet.statusMessage =
        ExcelSheetGridUtils::formatSheetStatus(sheet.grid.size(), sheet.columnCount, context.truncated, info.limits);
    return true;
}

} // namespace

bool inspectWorkbook(const QString& filePath, ExcelWorkbookInfo& info, QString& errorMessage)
{
    info = ExcelWorkbookInfo{};
    errorMessage.clear();

    const QString localPath = LocalFilePath::normalizeLocalFilePath(filePath);
    QFileInfo fileInfo;
    if (!ExcelFilePath::validateExcelFile(localPath, fileInfo, errorMessage)) {
        return false;
    }

    info.filePath = localPath;
    info.fileSizeBytes = fileInfo.size();
    info.limits = ExcelPreviewLimits::forFileSize(info.fileSizeBytes);
    info.previewMode = info.fileSizeBytes >= ExcelBackend::LargeFileThresholdBytes
                           && fileInfo.suffix().compare(QLatin1String("xls"), Qt::CaseInsensitive) != 0
                       ? ExcelPreviewMode::Streaming
                       : ExcelPreviewMode::Standard;

    if (info.previewMode == ExcelPreviewMode::Streaming) {
        return inspectWithXlsxio(localPath, info, errorMessage);
    }

    if (inspectWithQXlsx(localPath, info, errorMessage)) {
        return true;
    }

    if (fileInfo.suffix().compare(QLatin1String("xlsx"), Qt::CaseInsensitive) == 0) {
        QString xlsxioError;
        if (inspectWithXlsxio(localPath, info, xlsxioError)) {
            AppLogger::info(QStringLiteral("主解析器无法解析，已自动切换备用解析器：%1")
                                .arg(QFileInfo(localPath).fileName()));
            errorMessage.clear();
            info.previewMode = ExcelPreviewMode::Streaming;
            return true;
        }

        if (OoxmlSaxExcelLoader::inspectWorkbook(localPath, info.sheetNames, errorMessage)) {
            AppLogger::info(QStringLiteral("已使用兼容格式解析：%1")
                                .arg(QFileInfo(localPath).fileName()));
            info.previewMode = ExcelPreviewMode::AlternateXml;
            return true;
        }

        if (errorMessage.isEmpty()) {
            errorMessage = HANDLE_DATA_ERROR(
                fileInfo.fileName(),
                QStringLiteral("无法解析该 Excel 文件（可能是 Strict OOXML 格式）"));
        }
    }

    return false;
}

bool loadSheet(const ExcelWorkbookInfo& info,
               int sheetIndex,
               ExcelSheetPreview& sheet,
               QString& errorMessage)
{
    sheet = ExcelSheetPreview{};
    errorMessage.clear();

    if (info.filePath.isEmpty() || info.sheetNames.isEmpty()) {
        errorMessage = QStringLiteral("尚未打开工作簿");
        return false;
    }

    if (info.previewMode == ExcelPreviewMode::Streaming) {
        if (!loadSheetWithXlsxio(info, sheetIndex, sheet, errorMessage)) {
            return false;
        }
    } else if (info.previewMode == ExcelPreviewMode::AlternateXml) {
        if (!OoxmlSaxExcelLoader::loadSheetPreview(info.filePath,
                                                   sheetIndex,
                                                   info.limits,
                                                   sheet,
                                                   errorMessage)) {
            return false;
        }
    } else if (!loadSheetWithQXlsx(info, sheetIndex, sheet, errorMessage)) {
        return false;
    }

    return true;
}

} // namespace ExcelParserPreviewInternal
