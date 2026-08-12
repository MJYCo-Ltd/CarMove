#include "Business/BusinessExcelExporter.h"

#include "Business/BusinessWorkbookResolver.h"
#include "Core/LocalFilePath.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>
#include <QTextStream>
#include <QTime>

#include "xlsxdocument.h"
#include "xlsxformat.h"

QXLSX_USE_NAMESPACE

namespace {

QString fileNameForSheetWithExtension(const QString& excelFileName,
                                      const QString& sheetName,
                                      bool appendSheetName,
                                      const QString& extension)
{
    const QFileInfo fileInfo(excelFileName);
    QString baseName = fileInfo.completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("export");
    }

    if (!appendSheetName || sheetName.trimmed().isEmpty()) {
        return baseName + extension;
    }

    const QString sanitizedSheetName = LocalFilePath::sanitizeFileComponent(sheetName);
    if (sanitizedSheetName.isEmpty()) {
        return baseName + extension;
    }

    return baseName + QLatin1Char('-') + sanitizedSheetName + extension;
}

bool writeRowsToTemplateXlsx(const QList<BusinessExportRow>& rows,
                             const QString& filePath,
                             QString& errorMessage)
{
    Document document;
    const QString initialSheetName = document.sheetNames().value(0);
    if (!initialSheetName.isEmpty()) {
        document.renameSheet(initialSheetName, QStringLiteral("入参模板"));
    }

    Format headerFormat;
    headerFormat.setFontName(QStringLiteral("宋体"));
    headerFormat.setFontSize(14);
    headerFormat.setBorderStyle(Format::BorderThin);
    headerFormat.setHorizontalAlignment(Format::AlignLeft);
    headerFormat.setVerticalAlignment(Format::AlignVCenter);

    Format bodyFormat;
    bodyFormat.setFontName(QStringLiteral("宋体"));
    bodyFormat.setFontSize(14);
    bodyFormat.setHorizontalAlignment(Format::AlignLeft);
    bodyFormat.setVerticalAlignment(Format::AlignVCenter);

    Format dateTimeFormat = bodyFormat;
    dateTimeFormat.setNumberFormat(QStringLiteral("yyyy-mm-dd hh:mm:ss"));

    const QStringList headers = {
        QStringLiteral("车牌号"),
        QStringLiteral("车牌颜色(默认黄色)"),
        QStringLiteral("起始时间"),
        QStringLiteral("结束时间"),
        QStringLiteral("车辆类型(危货/普货 默认危货)"),
        QStringLiteral("是否跨天"),
        QStringLiteral("是否为半年内数据"),
    };
    for (int column = 0; column < headers.size(); ++column) {
        document.write(1, column + 1, headers.at(column), headerFormat);
    }

    document.setColumnWidth(1, 14.0);
    document.setColumnWidth(2, 24.0);
    document.setColumnWidth(3, 22.0);
    document.setColumnWidth(4, 22.0);
    document.setColumnWidth(5, 34.0);
    document.setColumnWidth(6, 12.0);
    document.setColumnWidth(7, 18.0);
    document.setRowHeight(1, 26.0);

    int outputRow = 2;
    for (const BusinessExportRow& row : rows) {
        const QDateTime startDateTime(row.startDate, QTime(0, 0, 0));
        const QDateTime endDateTime(row.endDate, QTime(23, 59, 59));
        const QString crossesDays = row.startDate == row.endDate ? QStringLiteral("否")
                                                                 : QStringLiteral("是");

        document.write(outputRow, 1, row.plate, bodyFormat);
        document.write(outputRow, 2, QStringLiteral("黄色"), bodyFormat);
        document.write(outputRow, 3, startDateTime, dateTimeFormat);
        document.write(outputRow, 4, endDateTime, dateTimeFormat);
        document.write(outputRow, 5, QStringLiteral("危货"), bodyFormat);
        document.write(outputRow, 6, crossesDays, bodyFormat);
        document.write(outputRow, 7, QStringLiteral("是"), bodyFormat);
        document.setRowHeight(outputRow, 22.0);
        ++outputRow;
    }

    if (!document.saveAs(filePath)) {
        errorMessage = QStringLiteral("无法写入 Excel 文件: %1").arg(filePath);
        return false;
    }

    return true;
}

} // namespace

QString BusinessExcelExporter::fileNameForSheet(const QString& excelFileName,
                                                  const QString& sheetName,
                                                  bool appendSheetName)
{
    const QFileInfo fileInfo(excelFileName);
    QString requestBaseName = fileInfo.completeBaseName();
    if (requestBaseName.isEmpty()) {
        requestBaseName = QStringLiteral("export");
    }
    requestBaseName += QStringLiteral("_请求");

    return fileNameForSheetWithExtension(requestBaseName,
                                         sheetName,
                                         appendSheetName,
                                         QStringLiteral(".xlsx"));
}

bool BusinessExcelExporter::exportRowsToXlsx(const QList<BusinessExportRow>& rows,
                                             const QString& filePath,
                                             const QString& sheetName,
                                             QString& errorMessage,
                                             int* exportedRows)
{
    errorMessage.clear();
    if (exportedRows != nullptr) {
        *exportedRows = 0;
    }

    if (rows.isEmpty()) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(
            sheetName,
            QStringLiteral("没有可导出的有效数据行（需包含车牌和有效日期）"));
        return false;
    }

    if (!writeRowsToTemplateXlsx(rows, filePath, errorMessage)) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName, errorMessage);
        return false;
    }

    if (exportedRows != nullptr) {
        *exportedRows = rows.size();
    }

    return true;
}

bool BusinessExcelExporter::writeAnomalyReport(const QStringList& anomalyMessages,
                                               const QString& sourceExcelFilePath,
                                               const QString& outputDirectory,
                                               QString& errorMessage,
                                               QString* reportFilePath)
{
    errorMessage.clear();
    const QString sourceBaseName = QFileInfo(sourceExcelFilePath).completeBaseName().isEmpty()
                                       ? QStringLiteral("export")
                                       : QFileInfo(sourceExcelFilePath).completeBaseName();
    const QString filePath = QDir(outputDirectory).filePath(
        sourceBaseName + QStringLiteral("_情况说明.txt"));
    if (reportFilePath != nullptr) {
        *reportFilePath = filePath;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        errorMessage = QStringLiteral("无法写入异常说明文件: %1").arg(filePath);
        return false;
    }

    file.write("\xEF\xBB\xBF");
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QStringLiteral("业务数据导出异常情况说明\n")
           << QStringLiteral("源文件：%1\n").arg(QFileInfo(sourceExcelFilePath).fileName())
           << QStringLiteral("异常数量：%1\n\n").arg(anomalyMessages.size());

    if (anomalyMessages.isEmpty()) {
        stream << QStringLiteral("未发现异常。\n");
    } else {
        for (int index = 0; index < anomalyMessages.size(); ++index) {
            stream << QStringLiteral("%1. %2\n").arg(index + 1).arg(anomalyMessages.at(index));
        }
    }

    return true;
}

bool BusinessExcelExporter::exportWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                                      const ExcelSheetPreview& referenceSheet,
                                                      int currentSheetIndex,
                                                      const BusinessColumnSelection& selection,
                                                      ExcelParserManager& parser,
                                                      const QString& outputDirectory,
                                                      QString& errorMessage,
                                                      int* exportedRows,
                                                      int* exportedFiles,
                                                      QStringList* exportedFilePaths,
                                                      QStringList* skippedSheetNames)
{
    errorMessage.clear();
    if (exportedRows != nullptr) {
        *exportedRows = 0;
    }
    if (exportedFiles != nullptr) {
        *exportedFiles = 0;
    }
    if (exportedFilePaths != nullptr) {
        exportedFilePaths->clear();
    }
    if (skippedSheetNames != nullptr) {
        skippedSheetNames->clear();
    }

    const QFileInfo outputDirInfo(outputDirectory);
    if (!outputDirInfo.exists() || !outputDirInfo.isDir()) {
        errorMessage = QStringLiteral("导出目录无效: %1").arg(outputDirectory);
        return false;
    }

    BusinessWorkbookRowsResult collected;
    if (!BusinessWorkbookResolver::collectWorkbookRows(workbookInfo,
                                                       referenceSheet,
                                                       currentSheetIndex,
                                                       selection,
                                                       parser,
                                                       collected,
                                                       errorMessage,
                                                       true)) {
        return false;
    }

    if (skippedSheetNames != nullptr) {
        *skippedSheetNames = collected.skippedSheetNames;
    }

    const QDir outputDir(outputDirectory);
    const QString excelFileName = QFileInfo(workbookInfo.filePath).fileName();
    const bool appendSheetName = BusinessWorkbookResolver::workbookUsesAllSheets(workbookInfo);

    int totalRows = 0;
    int fileCount = 0;

    for (const BusinessSheetRows& sheetRows : collected.sheets) {
        const QString fileName = fileNameForSheet(excelFileName, sheetRows.sheetName, appendSheetName);
        const QString filePath = outputDir.filePath(fileName);

        QString writeError;
        int sheetExportedRows = 0;
        if (!exportRowsToXlsx(sheetRows.rows,
                              filePath,
                              sheetRows.sheetName,
                              writeError,
                              &sheetExportedRows)) {
            errorMessage = writeError;
            return false;
        }

        totalRows += sheetExportedRows;
        ++fileCount;

        if (exportedFilePaths != nullptr) {
            exportedFilePaths->append(filePath);
        }
    }

    if (exportedRows != nullptr) {
        *exportedRows = totalRows;
    }
    if (exportedFiles != nullptr) {
        *exportedFiles = fileCount;
    }

    QString reportError;
    if (!writeAnomalyReport(collected.anomalyMessages,
                            workbookInfo.filePath,
                            outputDirectory,
                            reportError)) {
        errorMessage = reportError;
        return false;
    }

    return true;
}
