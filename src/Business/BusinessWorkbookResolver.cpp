#include "Business/BusinessWorkbookResolver.h"

#include "Business/BusinessColumnIdentifier.h"
#include "Business/DateColumnDetector.h"
#include "ExcelDriver/ExcelParserManager.h"
#include "Business/LicensePlateDetector.h"
#include "DataParsing/DateTimeParser.h"
#include "DataParsing/LicensePlate.h"

#include <QSet>

namespace {

BusinessExportOptions buildExportOptions(const ExcelSheetPreview& referenceSheet,
                                         const BusinessColumnSelection& selection)
{
    BusinessExportOptions options;
    options.dayOffset = selection.dayOffset;

    const bool hasSingleRole = !selection.singleTimeRole.isEmpty();
    options.singleDateColumn = hasSingleRole;
    if (hasSingleRole) {
        options.singleColumnIsStart = selection.singleTimeRole == QStringLiteral("start");
        const int dataColumn = (options.singleColumnIsStart ? selection.startColumnNumber
                                                            : selection.endColumnNumber)
                               - 1;
        const int ordinal = DateColumnDetector::ordinalForDataColumn(referenceSheet, dataColumn);
        if (options.singleColumnIsStart) {
            options.startDateOrdinal = ordinal;
        } else {
            options.endDateOrdinal = ordinal;
        }
    } else {
        options.startDateOrdinal =
            DateColumnDetector::ordinalForDataColumn(referenceSheet, selection.startColumnNumber - 1);
        options.endDateOrdinal =
            DateColumnDetector::ordinalForDataColumn(referenceSheet, selection.endColumnNumber - 1);
    }

    return options;
}

bool validateColumnSelection(const ExcelSheetPreview& referenceSheet,
                             const BusinessColumnSelection& selection,
                             QString& errorMessage)
{
    errorMessage.clear();

    if (LicensePlateDetector::firstColumnIndex(referenceSheet) < 0) {
        errorMessage = QStringLiteral("未检测到车牌列");
        return false;
    }

    const int dateColumnCount = DateColumnDetector::markedColumnIndices(referenceSheet).size();
    if (dateColumnCount <= 0) {
        errorMessage = QStringLiteral("未检测到日期列");
        return false;
    }

    return true;
}

bool validateResolvedColumns(const ResolvedSheetExportColumns& columns,
                             const BusinessExportOptions* options,
                             const QString& sheetName,
                             QString& errorMessage)
{
    if (columns.plateDataColumn < 0) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName,
                                                                  QStringLiteral("未检测到车牌列"));
        return false;
    }

    if (options == nullptr) {
        if (columns.startDataColumn < 0 || columns.endDataColumn < 0) {
            errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName,
                                                                      QStringLiteral("未检测到时间列"));
            return false;
        }
        return true;
    }

    if (options->singleDateColumn) {
        const int dateColumn =
            options->singleColumnIsStart ? columns.startDataColumn : columns.endDataColumn;
        if (dateColumn < 0) {
            errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName,
                                                                        QStringLiteral("未检测到时间列"));
            return false;
        }
        if (options->dayOffset < 0) {
            errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName,
                                                                      QStringLiteral("天数不能为负数"));
            return false;
        }
    } else {
        if (columns.startDataColumn < 0 || columns.endDataColumn < 0) {
            errorMessage = BusinessWorkbookResolver::formatSheetError(
                sheetName,
                QStringLiteral("未检测到开始时间或结束时间列"));
            return false;
        }
    }

    return true;
}

bool extractRowDates(const QVector<QString>& row,
                     const ResolvedSheetExportColumns& columns,
                     const BusinessExportOptions& options,
                     QDate& startDate,
                     QDate& endDate)
{
    if (options.singleDateColumn) {
        const int dateColumn =
            options.singleColumnIsStart ? columns.startDataColumn : columns.endDataColumn;
        if (dateColumn < 0 || dateColumn >= row.size()) {
            return false;
        }

        const QDate baseDate = DateTimeParser::parseToDate(row.at(dateColumn));
        if (!baseDate.isValid()) {
            return false;
        }

        if (options.singleColumnIsStart) {
            startDate = baseDate;
            endDate = baseDate.addDays(options.dayOffset);
        } else {
            endDate = baseDate;
            startDate = baseDate.addDays(-options.dayOffset);
        }
        return startDate.isValid() && endDate.isValid();
    }

    if (columns.startDataColumn < 0 || columns.endDataColumn < 0
        || columns.startDataColumn >= row.size() || columns.endDataColumn >= row.size()) {
        return false;
    }

    startDate = DateTimeParser::parseToDate(row.at(columns.startDataColumn));
    endDate = DateTimeParser::parseToDate(row.at(columns.endDataColumn));
    return startDate.isValid() && endDate.isValid();
}

QString extractPlateNumber(const QVector<QString>& row,
                           int plateDataColumn,
                           bool preserveSourceRows)
{
    if (plateDataColumn < 0 || plateDataColumn >= row.size()) {
        return QString();
    }

    const QString plate = LicensePlate::canonicalPlateNumber(row.at(plateDataColumn));
    if (plate.isEmpty()) {
        return QString();
    }

    if (preserveSourceRows) {
        return plate;
    }

    if (!LicensePlate::isChineseVehiclePlate(plate)) {
        return QString();
    }

    return plate;
}

ResolvedSheetExportColumns resolveColumns(const ExcelSheetPreview& sheet,
                                          const BusinessExportOptions& options)
{
    ResolvedSheetExportColumns columns;
    columns.plateDataColumn = LicensePlateDetector::firstColumnIndex(sheet);

    if (options.singleDateColumn) {
        const int ordinal =
            options.singleColumnIsStart ? options.startDateOrdinal : options.endDateOrdinal;
        const int dateColumn = DateColumnDetector::dataColumnAtOrdinal(sheet, ordinal);
        if (options.singleColumnIsStart) {
            columns.startDataColumn = dateColumn;
            columns.endDataColumn = dateColumn;
        } else {
            columns.endDataColumn = dateColumn;
            columns.startDataColumn = dateColumn;
        }
    } else {
        columns.startDataColumn =
            DateColumnDetector::dataColumnAtOrdinal(sheet, options.startDateOrdinal);
        columns.endDataColumn =
            DateColumnDetector::dataColumnAtOrdinal(sheet, options.endDateOrdinal);
    }

    return columns;
}

ResolvedSheetExportColumns resolveColumnsAuto(const ExcelSheetPreview& sheet)
{
    ResolvedSheetExportColumns columns;
    columns.plateDataColumn = LicensePlateDetector::firstColumnIndex(sheet);

    const QList<int> dateColumns = DateColumnDetector::markedColumnIndices(sheet);
    if (dateColumns.isEmpty()) {
        return columns;
    }

    columns.startDataColumn = dateColumns.first();
    columns.endDataColumn = dateColumns.size() >= 2 ? dateColumns.at(1) : dateColumns.first();
    return columns;
}

QList<BusinessExportRow> collectSheetRows(const ExcelSheetPreview& sheet,
                                          const ResolvedSheetExportColumns& columns,
                                          const BusinessExportOptions& options,
                                          bool preserveSourceRows = false,
                                          QStringList* anomalyMessages = nullptr)
{
    QList<BusinessExportRow> rows;
    rows.reserve(sheet.grid.size());
    QSet<QString> seenRows;

    for (int rowIndex = 0; rowIndex < sheet.grid.size(); ++rowIndex) {
        const QVector<QString>& gridRow = sheet.grid.at(rowIndex);
        const int excelRowNumber = rowIndex + 1;
        bool rowHasData = false;
        for (const QString& value : gridRow) {
            if (!value.trimmed().isEmpty()) {
                rowHasData = true;
                break;
            }
        }
        if (!rowHasData) {
            continue;
        }

        const QString rawPlate = columns.plateDataColumn >= 0
                                     && columns.plateDataColumn < gridRow.size()
                                 ? gridRow.at(columns.plateDataColumn).trimmed()
                                 : QString();
        const bool isHeaderRow = rowIndex < 3
                                 && (rawPlate.contains(QStringLiteral("车牌"))
                                     || rawPlate.contains(QStringLiteral("牌照"))
                                     || rawPlate.contains(QStringLiteral("车号")));
        if (isHeaderRow) {
            continue;
        }

        const QString plate = extractPlateNumber(gridRow,
                                                 columns.plateDataColumn,
                                                 preserveSourceRows);
        if (plate.isEmpty()) {
            if (preserveSourceRows && anomalyMessages != nullptr) {
                anomalyMessages->append(
                    QStringLiteral("工作表「%1」第 %2 行：车牌为空，未导出。")
                        .arg(sheet.name)
                        .arg(excelRowNumber));
            }
            continue;
        }

        if (preserveSourceRows && anomalyMessages != nullptr) {
            if (rawPlate != plate) {
                anomalyMessages->append(
                    QStringLiteral("工作表「%1」第 %2 行：车牌“%3”已规范为“%4”。")
                        .arg(sheet.name)
                        .arg(excelRowNumber)
                        .arg(rawPlate, plate));
            }
            if (!LicensePlate::isChineseVehiclePlate(plate)) {
                anomalyMessages->append(
                    QStringLiteral("工作表「%1」第 %2 行：车牌“%3”不符合标准格式，已按原值导出。")
                        .arg(sheet.name)
                        .arg(excelRowNumber)
                        .arg(plate));
            }
        }

        QDate startDate;
        QDate endDate;
        if (!extractRowDates(gridRow, columns, options, startDate, endDate)) {
            if (preserveSourceRows && anomalyMessages != nullptr) {
                anomalyMessages->append(
                    QStringLiteral("工作表「%1」第 %2 行：时间数据无效，未导出。")
                        .arg(sheet.name)
                        .arg(excelRowNumber));
            }
            continue;
        }

        if (preserveSourceRows && anomalyMessages != nullptr) {
            const QString rowKey = plate.toUpper() + QLatin1Char('|')
                                   + startDate.toString(Qt::ISODate) + QLatin1Char('|')
                                   + endDate.toString(Qt::ISODate);
            if (seenRows.contains(rowKey)) {
                anomalyMessages->append(
                    QStringLiteral("工作表「%1」第 %2 行：车牌“%3”与起止时间重复，未导出该记录。")
                        .arg(sheet.name)
                        .arg(excelRowNumber)
                        .arg(plate));
                continue;
            } else {
                seenRows.insert(rowKey);
            }
        }

        rows.append(BusinessExportRow{plate, startDate, endDate});
    }

    return rows;
}

} // namespace

BusinessColumnSelection BusinessColumnSelection::fromUi(int startColumnNumber,
                                                        int endColumnNumber,
                                                        const QString& singleTimeRole,
                                                        int dayOffset)
{
    BusinessColumnSelection selection;
    selection.startColumnNumber = startColumnNumber;
    selection.endColumnNumber = endColumnNumber;
    selection.singleTimeRole = singleTimeRole;
    selection.dayOffset = dayOffset;
    return selection;
}

int BusinessWorkbookRowsResult::totalRowCount() const
{
    int count = 0;
    for (const BusinessSheetRows& sheetRows : sheets) {
        count += sheetRows.rows.size();
    }
    return count;
}

QList<BusinessExportRow> BusinessWorkbookRowsResult::allRowsFlat() const
{
    QList<BusinessExportRow> rows;
    for (const BusinessSheetRows& sheetRows : sheets) {
        rows.append(sheetRows.rows);
    }
    return rows;
}

QString BusinessWorkbookResolver::formatSheetError(const QString& sheetName, const QString& message)
{
    if (sheetName.trimmed().isEmpty()) {
        return message;
    }
    return QStringLiteral("工作表「%1」：%2").arg(sheetName.trimmed(), message);
}

bool BusinessWorkbookResolver::workbookUsesAllSheets(const ExcelWorkbookInfo& workbookInfo)
{
    return workbookInfo.sheetNames.size() > 1;
}

bool BusinessWorkbookResolver::collectWorkbookRows(const ExcelWorkbookInfo& workbookInfo,
                                                   const ExcelSheetPreview& referenceSheet,
                                                   int currentSheetIndex,
                                                   const BusinessColumnSelection& selection,
                                                   ExcelParserManager& parser,
                                                   BusinessWorkbookRowsResult& result,
                                                   QString& errorMessage,
                                                   bool preserveSourceRows)
{
    result.sheets.clear();
    result.skippedSheetNames.clear();
    result.anomalyMessages.clear();
    errorMessage.clear();

    if (!validateColumnSelection(referenceSheet, selection, errorMessage)) {
        return false;
    }

    const BusinessExportOptions options = buildExportOptions(referenceSheet, selection);
    const bool allSheets = workbookUsesAllSheets(workbookInfo);

    if (allSheets) {
        if (workbookInfo.sheetNames.isEmpty()) {
            errorMessage = QStringLiteral("工作簿中没有可处理的工作表");
            return false;
        }

        for (int sheetIndex = 0; sheetIndex < workbookInfo.sheetNames.size(); ++sheetIndex) {
            const QString sheetName = workbookInfo.sheetNames.at(sheetIndex);

            ExcelWorkbookInfo exportInfo = workbookInfo;
            exportInfo.limits = ExcelPreviewLimits::forExport();

            ExcelSheetPreview sheet;
            QString sheetError;
            if (!parser.loadSheet(exportInfo, sheetIndex, sheet, sheetError)) {
                errorMessage = formatSheetError(
                    sheetName,
                    sheetError.isEmpty() ? QStringLiteral("无法加载工作表") : sheetError);
                return false;
            }

            BusinessColumnIdentifier::identifyColumns(sheet);

            const ResolvedSheetExportColumns columns = resolveColumnsAuto(sheet);
            QString validationError;
            if (!validateResolvedColumns(columns, nullptr, sheetName, validationError)) {
                result.skippedSheetNames.append(sheetName);
                continue;
            }

            QList<BusinessExportRow> rows = collectSheetRows(sheet,
                                                             columns,
                                                             options,
                                                             preserveSourceRows,
                                                             preserveSourceRows
                                                                 ? &result.anomalyMessages
                                                                 : nullptr);
            if (rows.isEmpty()) {
                result.skippedSheetNames.append(sheetName);
                continue;
            }

            result.sheets.append(BusinessSheetRows{sheet.name, rows});
        }

        if (result.sheets.isEmpty()) {
            errorMessage = QStringLiteral("没有有效的业务数据行（需包含车牌和有效日期）");
            return false;
        }

        return true;
    }

    ExcelWorkbookInfo exportInfo = workbookInfo;
    exportInfo.limits = ExcelPreviewLimits::forExport();

    ExcelSheetPreview sheet;
    QString sheetError;
    if (!parser.loadSheet(exportInfo, currentSheetIndex, sheet, sheetError)) {
        errorMessage = sheetError.isEmpty() ? QStringLiteral("无法加载工作表数据") : sheetError;
        return false;
    }

    BusinessColumnIdentifier::identifyColumns(sheet);

    const ResolvedSheetExportColumns columns = resolveColumns(sheet, options);
    if (!validateResolvedColumns(columns, &options, sheet.name, errorMessage)) {
        return false;
    }

    QList<BusinessExportRow> rows = collectSheetRows(sheet,
                                                     columns,
                                                     options,
                                                     preserveSourceRows,
                                                     preserveSourceRows
                                                         ? &result.anomalyMessages
                                                         : nullptr);
    if (rows.isEmpty()) {
        errorMessage = QStringLiteral("没有有效的业务数据行（需包含车牌和有效日期）");
        return false;
    }

    result.sheets.append(BusinessSheetRows{sheet.name, rows});
    return true;
}

QList<int> BusinessWorkbookResolver::processableSheetIndices(const ExcelWorkbookInfo& workbookInfo,
                                                             int currentSheetIndex)
{
    QList<int> indices;
    if (workbookUsesAllSheets(workbookInfo)) {
        indices.reserve(workbookInfo.sheetNames.size());
        for (int i = 0; i < workbookInfo.sheetNames.size(); ++i) {
            indices.append(i);
        }
    } else if (currentSheetIndex >= 0 && currentSheetIndex < workbookInfo.sheetNames.size()) {
        indices.append(currentSheetIndex);
    }
    return indices;
}

bool BusinessWorkbookResolver::collectSheetBusinessRows(const ExcelWorkbookInfo& workbookInfo,
                                                         const ExcelSheetPreview& referenceSheet,
                                                         int targetSheetIndex,
                                                         const BusinessColumnSelection& selection,
                                                         ExcelParserManager& parser,
                                                         BusinessSheetRows& outSheetRows,
                                                         QString& errorMessage,
                                                         bool* sheetSkipped)
{
    if (sheetSkipped != nullptr) {
        *sheetSkipped = false;
    }
    outSheetRows = BusinessSheetRows{};
    errorMessage.clear();

    if (targetSheetIndex < 0 || targetSheetIndex >= workbookInfo.sheetNames.size()) {
        errorMessage = QStringLiteral("工作表索引无效");
        return false;
    }

    if (!validateColumnSelection(referenceSheet, selection, errorMessage)) {
        return false;
    }

    const BusinessExportOptions options = buildExportOptions(referenceSheet, selection);
    const bool allSheets = workbookUsesAllSheets(workbookInfo);
    const QString sheetName = workbookInfo.sheetNames.at(targetSheetIndex);

    ExcelWorkbookInfo exportInfo = workbookInfo;
    exportInfo.limits = ExcelPreviewLimits::forExport();

    ExcelSheetPreview sheet;
    QString sheetError;
    if (!parser.loadSheet(exportInfo, targetSheetIndex, sheet, sheetError)) {
        errorMessage = formatSheetError(
            sheetName,
            sheetError.isEmpty() ? QStringLiteral("无法加载工作表") : sheetError);
        return false;
    }

    BusinessColumnIdentifier::identifyColumns(sheet);

    ResolvedSheetExportColumns columns;
    if (allSheets) {
        columns = resolveColumnsAuto(sheet);
        QString validationError;
        if (!validateResolvedColumns(columns, nullptr, sheetName, validationError)) {
            if (sheetSkipped != nullptr) {
                *sheetSkipped = true;
            }
            return true;
        }
    } else {
        columns = resolveColumns(sheet, options);
        if (!validateResolvedColumns(columns, &options, sheet.name, errorMessage)) {
            return false;
        }
    }

    const QList<BusinessExportRow> rows = collectSheetRows(sheet, columns, options);
    if (rows.isEmpty()) {
        if (sheetSkipped != nullptr) {
            *sheetSkipped = true;
        }
        return true;
    }

    outSheetRows.sheetName = sheet.name;
    outSheetRows.rows = rows;
    return true;
}
