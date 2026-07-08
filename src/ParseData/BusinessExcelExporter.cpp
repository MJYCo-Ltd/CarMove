#include "ParseData/BusinessExcelExporter.h"

#include "ParseData/DateColumnDetector.h"
#include "ParseData/ExcelPreviewLoader.h"
#include "ParseData/LicensePlateDetector.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

namespace {

QString csvField(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));

    if (escaped.contains(QLatin1Char(',')) || escaped.contains(QLatin1Char('"'))
        || escaped.contains(QLatin1Char('\n')) || escaped.contains(QLatin1Char('\r'))) {
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }

    return escaped;
}

QString formatExportDate(const QDate& date)
{
    if (!date.isValid()) {
        return QString();
    }
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

QString sanitizeFileComponent(const QString& name)
{
    QString sanitized = name.trimmed();
    sanitized.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    return sanitized;
}

QString withSheetContext(const QString& sheetName, const QString& message)
{
    if (sheetName.trimmed().isEmpty()) {
        return message;
    }
    return QStringLiteral("工作表「%1」：%2").arg(sheetName.trimmed(), message);
}

bool validateResolvedColumns(const ResolvedSheetExportColumns& columns,
                             const BusinessExportOptions* options,
                             const QString& sheetName,
                             QString& errorMessage)
{
    if (columns.plateDataColumn < 0) {
        errorMessage = withSheetContext(sheetName, QStringLiteral("未检测到车牌列"));
        return false;
    }

    if (options == nullptr) {
        if (columns.startDataColumn < 0 || columns.endDataColumn < 0) {
            errorMessage = withSheetContext(sheetName, QStringLiteral("未检测到时间列"));
            return false;
        }
        return true;
    }

    if (options->singleDateColumn) {
        const int dateColumn =
            options->singleColumnIsStart ? columns.startDataColumn : columns.endDataColumn;
        if (dateColumn < 0) {
            errorMessage = withSheetContext(sheetName, QStringLiteral("未检测到时间列"));
            return false;
        }
        if (options->dayOffset < 0) {
            errorMessage = withSheetContext(sheetName, QStringLiteral("天数不能为负数"));
            return false;
        }
    } else {
        if (columns.startDataColumn < 0 || columns.endDataColumn < 0) {
            errorMessage =
                withSheetContext(sheetName, QStringLiteral("未检测到开始时间或结束时间列"));
            return false;
        }
        if (columns.startDataColumn == columns.endDataColumn) {
            errorMessage =
                withSheetContext(sheetName, QStringLiteral("开始时间和结束时间不能选择同一列"));
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

        const QDate baseDate = DateColumnDetector::parseToDate(row.at(dateColumn));
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

    startDate = DateColumnDetector::parseToDate(row.at(columns.startDataColumn));
    endDate = DateColumnDetector::parseToDate(row.at(columns.endDataColumn));
    return startDate.isValid() && endDate.isValid();
}

bool writeRowsToCsv(const QList<BusinessExportRow>& rows,
                    const QString& filePath,
                    QString& errorMessage)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = QStringLiteral("无法写入文件: %1").arg(filePath);
        return false;
    }

    file.write("\xEF\xBB\xBF");

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QStringLiteral("车牌,开始时间,结束时间\n");

    for (const BusinessExportRow& row : rows) {
        stream << csvField(row.plate) << QLatin1Char(',') << csvField(formatExportDate(row.startDate))
               << QLatin1Char(',') << csvField(formatExportDate(row.endDate)) << QLatin1Char('\n');
    }

    return true;
}

bool prepareAndWriteCsv(QList<BusinessExportRow> rows,
                        const QString& filePath,
                        const QString& sheetName,
                        QString& errorMessage,
                        int* exportedRows)
{
    if (rows.isEmpty()) {
        errorMessage = withSheetContext(
            sheetName,
            QStringLiteral("没有可导出的有效数据行（需包含车牌和有效日期）"));
        return false;
    }

    BusinessExcelExporter::sortRowsByPlate(rows);
    BusinessExcelExporter::deduplicateRows(rows);

    if (!writeRowsToCsv(rows, filePath, errorMessage)) {
        errorMessage = withSheetContext(sheetName, errorMessage);
        return false;
    }

    if (exportedRows != nullptr) {
        *exportedRows = rows.size();
    }

    return true;
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

QString trajectoryLookupKey(const QString& plate, const QDate& startDate, const QDate& endDate)
{
    return plate.trimmed().toUpper() + QLatin1Char('|') + formatExportDate(startDate)
           + QLatin1Char('|') + formatExportDate(endDate);
}

QString trajectoryFileBaseName(const QString& plate, const QDate& startDate, const QDate& endDate)
{
    return plate.trimmed() + QLatin1Char('-') + formatExportDate(startDate) + QLatin1Char('-')
           + formatExportDate(endDate);
}

QHash<QString, QString> indexTrajectoryFiles(const QString& trajectoryDirectory,
                                               QString& errorMessage)
{
    QHash<QString, QString> index;
    errorMessage.clear();

    const QFileInfo dirInfo(trajectoryDirectory);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        errorMessage = QStringLiteral("轨迹目录无效: %1").arg(trajectoryDirectory);
        return index;
    }

    const QDir dir(trajectoryDirectory);
    const QStringList filters{QStringLiteral("*.xlsx"),
                              QStringLiteral("*.xls"),
                              QStringLiteral("*.XLSX"),
                              QStringLiteral("*.XLS")};
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable);

    const QRegularExpression fileNameRegex(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})-(\\d{4}-\\d{2}-\\d{2})-(\\d{4}-\\d{2}-\\d{2})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);

    for (const QFileInfo& fileInfo : files) {
        const QRegularExpressionMatch match = fileNameRegex.match(fileInfo.fileName());
        if (!match.hasMatch()) {
            continue;
        }

        const QString plate = match.captured(1);
        const QDate startDate = QDate::fromString(match.captured(2), QStringLiteral("yyyy-MM-dd"));
        const QDate endDate = QDate::fromString(match.captured(3), QStringLiteral("yyyy-MM-dd"));
        if (!startDate.isValid() || !endDate.isValid()) {
            continue;
        }

        const QString key = trajectoryLookupKey(plate, startDate, endDate);
        if (!index.contains(key)) {
            index.insert(key, fileInfo.absoluteFilePath());
        }
    }

    return index;
}

bool moveTrajectoryFile(const QString& sourcePath, const QString& destinationPath, QString& errorMessage)
{
    const QFileInfo destinationInfo(destinationPath);
    if (!QDir().mkpath(destinationInfo.absolutePath())) {
        errorMessage = QStringLiteral("无法创建目录: %1").arg(destinationInfo.absolutePath());
        return false;
    }

    if (QFile::exists(destinationPath)) {
        errorMessage = QStringLiteral("目标文件已存在: %1").arg(destinationPath);
        return false;
    }

    if (QFile::rename(sourcePath, destinationPath)) {
        return true;
    }

    if (!QFile::copy(sourcePath, destinationPath)) {
        errorMessage = QStringLiteral("移动失败: %1").arg(sourcePath);
        return false;
    }

    if (!QFile::remove(sourcePath)) {
        QFile::remove(destinationPath);
        errorMessage = QStringLiteral("移动失败：无法删除源文件 %1").arg(sourcePath);
        return false;
    }

    return true;
}

bool classifyRowsIntoFolder(const QList<BusinessExportRow>& rows,
                            const QHash<QString, QString>& trajectoryIndex,
                            const QString& targetFolderPath,
                            QString& errorMessage,
                            int* movedFiles,
                            int* missingFiles,
                            QStringList* missingEntries)
{
    if (movedFiles != nullptr) {
        *movedFiles = 0;
    }
    if (missingFiles != nullptr) {
        *missingFiles = 0;
    }
    if (missingEntries != nullptr) {
        missingEntries->clear();
    }

    if (!QDir().mkpath(targetFolderPath)) {
        errorMessage = QStringLiteral("无法创建归类目录: %1").arg(targetFolderPath);
        return false;
    }

    const QDir targetDir(targetFolderPath);
    int moved = 0;
    int missing = 0;

    for (const BusinessExportRow& row : rows) {
        const QString key = trajectoryLookupKey(row.plate, row.startDate, row.endDate);
        const auto indexIt = trajectoryIndex.constFind(key);
        if (indexIt == trajectoryIndex.constEnd()) {
            ++missing;
            if (missingEntries != nullptr) {
                missingEntries->append(trajectoryFileBaseName(row.plate, row.startDate, row.endDate)
                                       + QStringLiteral(".xlsx"));
            }
            continue;
        }

        const QFileInfo sourceInfo(indexIt.value());
        const QString destinationPath =
            targetDir.filePath(sourceInfo.fileName());

        QString moveError;
        if (!moveTrajectoryFile(indexIt.value(), destinationPath, moveError)) {
            errorMessage = moveError;
            return false;
        }

        ++moved;
    }

    if (movedFiles != nullptr) {
        *movedFiles = moved;
    }
    if (missingFiles != nullptr) {
        *missingFiles = missing;
    }

    return true;
}

} // namespace

QString BusinessExcelExporter::fileNameForSheet(const QString& excelFileName,
                                                  const QString& sheetName,
                                                  bool appendSheetName)
{
    const QFileInfo fileInfo(excelFileName);
    QString baseName = fileInfo.completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("export");
    }

    if (!appendSheetName || sheetName.trimmed().isEmpty()) {
        return baseName + QStringLiteral(".csv");
    }

    const QString sanitizedSheetName = sanitizeFileComponent(sheetName);
    if (sanitizedSheetName.isEmpty()) {
        return baseName + QStringLiteral(".csv");
    }

    return baseName + QLatin1Char('-') + sanitizedSheetName + QStringLiteral(".csv");
}

QString BusinessExcelExporter::folderNameForSheet(const QString& excelFileName,
                                                  const QString& sheetName,
                                                  bool appendSheetName)
{
    const QString csvFileName = fileNameForSheet(excelFileName, sheetName, appendSheetName);
    return QFileInfo(csvFileName).completeBaseName();
}

ResolvedSheetExportColumns BusinessExcelExporter::resolveColumns(
    const ExcelSheetPreview& sheet,
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
        } else {
            columns.endDataColumn = dateColumn;
        }
    } else {
        columns.startDataColumn =
            DateColumnDetector::dataColumnAtOrdinal(sheet, options.startDateOrdinal);
        columns.endDataColumn =
            DateColumnDetector::dataColumnAtOrdinal(sheet, options.endDateOrdinal);
    }

    return columns;
}

QList<BusinessExportRow> BusinessExcelExporter::collectRows(const ExcelSheetPreview& sheet,
                                                            const ResolvedSheetExportColumns& columns,
                                                            const BusinessExportOptions& options)
{
    QList<BusinessExportRow> rows;

    if (sheet.grid.isEmpty()) {
        return rows;
    }

    for (const QVector<QString>& gridRow : sheet.grid) {
        if (columns.plateDataColumn < 0 || columns.plateDataColumn >= gridRow.size()) {
            continue;
        }

        const QString plate = gridRow.at(columns.plateDataColumn).trimmed();
        if (!LicensePlateDetector::isChineseVehiclePlate(plate)) {
            continue;
        }

        QDate startDate;
        QDate endDate;
        if (!extractRowDates(gridRow, columns, options, startDate, endDate)) {
            continue;
        }

        rows.append(BusinessExportRow{plate, startDate, endDate});
    }

    return rows;
}

void BusinessExcelExporter::sortRowsByPlate(QList<BusinessExportRow>& rows)
{
    std::stable_sort(rows.begin(), rows.end(), [](const BusinessExportRow& left,
                                                  const BusinessExportRow& right) {
        const int plateOrder = QString::compare(left.plate, right.plate, Qt::CaseInsensitive);
        if (plateOrder != 0) {
            return plateOrder < 0;
        }
        if (left.startDate != right.startDate) {
            return left.startDate < right.startDate;
        }
        return left.endDate < right.endDate;
    });
}

void BusinessExcelExporter::deduplicateRows(QList<BusinessExportRow>& rows)
{
    if (rows.size() <= 1) {
        return;
    }

    const auto duplicateBegin = std::unique(rows.begin(),
                                            rows.end(),
                                            [](const BusinessExportRow& left,
                                               const BusinessExportRow& right) {
                                                return QString::compare(left.plate,
                                                                        right.plate,
                                                                        Qt::CaseInsensitive)
                                                           == 0
                                                    && left.startDate == right.startDate
                                                    && left.endDate == right.endDate;
                                            });
    rows.erase(duplicateBegin, rows.end());
}

QString BusinessExcelExporter::formatSheetError(const QString& sheetName, const QString& message)
{
    return withSheetContext(sheetName, message);
}

bool BusinessExcelExporter::exportSheetToCsv(const ExcelSheetPreview& sheet,
                                             const BusinessExportOptions& options,
                                             const QString& filePath,
                                             QString& errorMessage,
                                             int* exportedRows)
{
    errorMessage.clear();
    if (exportedRows != nullptr) {
        *exportedRows = 0;
    }

    const ResolvedSheetExportColumns columns = resolveColumns(sheet, options);
    if (!validateResolvedColumns(columns, &options, sheet.name, errorMessage)) {
        return false;
    }

    QList<BusinessExportRow> rows = collectRows(sheet, columns, options);
    return prepareAndWriteCsv(rows, filePath, sheet.name, errorMessage, exportedRows);
}

bool BusinessExcelExporter::exportWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                                      const BusinessExportOptions& options,
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

    if (workbookInfo.sheetNames.isEmpty()) {
        errorMessage = QStringLiteral("工作簿中没有可导出的工作表");
        return false;
    }

    const QFileInfo outputDirInfo(outputDirectory);
    if (!outputDirInfo.exists() || !outputDirInfo.isDir()) {
        errorMessage = QStringLiteral("导出目录无效: %1").arg(outputDirectory);
        return false;
    }

    const QDir outputDir(outputDirectory);
    const QString excelFileName = QFileInfo(workbookInfo.filePath).fileName();

    int totalRows = 0;
    int fileCount = 0;

    for (int sheetIndex = 0; sheetIndex < workbookInfo.sheetNames.size(); ++sheetIndex) {
        const QString sheetName = workbookInfo.sheetNames.at(sheetIndex);

        ExcelWorkbookInfo exportInfo = workbookInfo;
        exportInfo.limits = ExcelPreviewLimits::forExport();

        ExcelSheetPreview sheet;
        QString sheetError;
        if (!ExcelPreviewLoader::loadSheet(exportInfo, sheetIndex, sheet, sheetError)) {
            errorMessage = withSheetContext(
                sheetName,
                sheetError.isEmpty() ? QStringLiteral("无法加载工作表") : sheetError);
            return false;
        }

        const ResolvedSheetExportColumns columns = resolveColumnsAuto(sheet);
        QString validationError;
        if (!validateResolvedColumns(columns, nullptr, sheetName, validationError)) {
            if (skippedSheetNames != nullptr) {
                skippedSheetNames->append(sheetName);
            }
            continue;
        }

        QList<BusinessExportRow> rows = collectRows(sheet, columns, options);
        if (rows.isEmpty()) {
            if (skippedSheetNames != nullptr) {
                skippedSheetNames->append(sheetName);
            }
            continue;
        }

        const QString fileName =
            fileNameForSheet(excelFileName, sheet.name, workbookInfo.sheetNames.size() > 1);
        const QString filePath = outputDir.filePath(fileName);

        QString writeError;
        int sheetExportedRows = 0;
        if (!prepareAndWriteCsv(rows, filePath, sheetName, writeError, &sheetExportedRows)) {
            errorMessage = writeError;
            return false;
        }

        totalRows += sheetExportedRows;
        ++fileCount;

        if (exportedFilePaths != nullptr) {
            exportedFilePaths->append(filePath);
        }
    }

    if (fileCount == 0) {
        errorMessage = QStringLiteral("没有可导出的有效数据行（需包含车牌和有效日期）");
        return false;
    }

    if (exportedRows != nullptr) {
        *exportedRows = totalRows;
    }
    if (exportedFiles != nullptr) {
        *exportedFiles = fileCount;
    }

    return true;
}

bool BusinessExcelExporter::classifySheet(const ExcelSheetPreview& sheet,
                                            const BusinessExportOptions& options,
                                            const QString& trajectoryDirectory,
                                            const QString& outputDirectory,
                                            const QString& excelFileName,
                                            bool appendSheetName,
                                            QString& errorMessage,
                                            BusinessClassifyResult* result)
{
    errorMessage.clear();
    if (result != nullptr) {
        *result = BusinessClassifyResult{};
    }

    const QFileInfo outputDirInfo(outputDirectory);
    if (!outputDirInfo.exists() || !outputDirInfo.isDir()) {
        errorMessage = QStringLiteral("输出目录无效: %1").arg(outputDirectory);
        return false;
    }

    QString trajectoryIndexError;
    const QHash<QString, QString> trajectoryIndex =
        indexTrajectoryFiles(trajectoryDirectory, trajectoryIndexError);
    if (!trajectoryIndexError.isEmpty()) {
        errorMessage = trajectoryIndexError;
        return false;
    }

    if (trajectoryIndex.isEmpty()) {
        errorMessage = QStringLiteral("轨迹目录中没有找到符合命名规则的文件（车牌-开始日期-结束日期.xlsx）");
        return false;
    }

    const ResolvedSheetExportColumns columns = resolveColumns(sheet, options);
    if (!validateResolvedColumns(columns, &options, sheet.name, errorMessage)) {
        return false;
    }

    QList<BusinessExportRow> rows = collectRows(sheet, columns, options);
    if (rows.isEmpty()) {
        errorMessage = withSheetContext(
            sheet.name,
            QStringLiteral("没有可归类的有效数据行（需包含车牌和有效日期）"));
        return false;
    }

    sortRowsByPlate(rows);
    deduplicateRows(rows);

    const QDir outputDir(outputDirectory);
    const QString csvFileName = fileNameForSheet(excelFileName, sheet.name, appendSheetName);
    const QString csvFilePath = outputDir.filePath(csvFileName);

    QString writeError;
    if (!writeRowsToCsv(rows, csvFilePath, writeError)) {
        errorMessage = withSheetContext(sheet.name, writeError);
        return false;
    }

    const QString folderName = folderNameForSheet(excelFileName, sheet.name, appendSheetName);
    const QString targetFolderPath = outputDir.filePath(folderName);

    int movedFiles = 0;
    int missingFiles = 0;
    QStringList missingEntries;
    QString classifyError;
    if (!classifyRowsIntoFolder(rows,
                                trajectoryIndex,
                                targetFolderPath,
                                classifyError,
                                &movedFiles,
                                &missingFiles,
                                &missingEntries)) {
        errorMessage = withSheetContext(sheet.name, classifyError);
        return false;
    }

    if (result != nullptr) {
        result->movedFiles = movedFiles;
        result->missingFiles = missingFiles;
        result->exportedCsvFiles = 1;
        result->exportedRows = rows.size();
        result->missingEntries = missingEntries;
    }

    return true;
}

bool BusinessExcelExporter::classifyWorkbookToDirectory(
    const ExcelWorkbookInfo& workbookInfo,
    const BusinessExportOptions& options,
    const QString& trajectoryDirectory,
    const QString& outputDirectory,
    QString& errorMessage,
    BusinessClassifyResult* result)
{
    errorMessage.clear();
    if (result != nullptr) {
        *result = BusinessClassifyResult{};
    }

    if (workbookInfo.sheetNames.isEmpty()) {
        errorMessage = QStringLiteral("工作簿中没有可归类的工作表");
        return false;
    }

    const QFileInfo outputDirInfo(outputDirectory);
    if (!outputDirInfo.exists() || !outputDirInfo.isDir()) {
        errorMessage = QStringLiteral("输出目录无效: %1").arg(outputDirectory);
        return false;
    }

    QString trajectoryIndexError;
    const QHash<QString, QString> trajectoryIndex =
        indexTrajectoryFiles(trajectoryDirectory, trajectoryIndexError);
    if (!trajectoryIndexError.isEmpty()) {
        errorMessage = trajectoryIndexError;
        return false;
    }

    if (trajectoryIndex.isEmpty()) {
        errorMessage = QStringLiteral("轨迹目录中没有找到符合命名规则的文件（车牌-开始日期-结束日期.xlsx）");
        return false;
    }

    const QString excelFileName = QFileInfo(workbookInfo.filePath).fileName();
    const bool appendSheetName = workbookInfo.sheetNames.size() > 1;
    const QDir outputDir(outputDirectory);

    BusinessClassifyResult aggregate;
    int processedSheets = 0;

    for (int sheetIndex = 0; sheetIndex < workbookInfo.sheetNames.size(); ++sheetIndex) {
        const QString sheetName = workbookInfo.sheetNames.at(sheetIndex);

        ExcelWorkbookInfo exportInfo = workbookInfo;
        exportInfo.limits = ExcelPreviewLimits::forExport();

        ExcelSheetPreview sheet;
        QString sheetError;
        if (!ExcelPreviewLoader::loadSheet(exportInfo, sheetIndex, sheet, sheetError)) {
            errorMessage = withSheetContext(
                sheetName,
                sheetError.isEmpty() ? QStringLiteral("无法加载工作表") : sheetError);
            return false;
        }

        const ResolvedSheetExportColumns columns = resolveColumnsAuto(sheet);
        QString validationError;
        if (!validateResolvedColumns(columns, nullptr, sheetName, validationError)) {
            aggregate.skippedSheetNames.append(sheetName);
            continue;
        }

        QList<BusinessExportRow> rows = collectRows(sheet, columns, options);
        if (rows.isEmpty()) {
            aggregate.skippedSheetNames.append(sheetName);
            continue;
        }

        sortRowsByPlate(rows);
        deduplicateRows(rows);

        const QString csvFileName = fileNameForSheet(excelFileName, sheet.name, appendSheetName);
        const QString csvFilePath = outputDir.filePath(csvFileName);

        QString writeError;
        if (!writeRowsToCsv(rows, csvFilePath, writeError)) {
            errorMessage = withSheetContext(sheetName, writeError);
            return false;
        }

        const QString folderName = folderNameForSheet(excelFileName, sheet.name, appendSheetName);
        const QString targetFolderPath = outputDir.filePath(folderName);

        int movedFiles = 0;
        int missingFiles = 0;
        QStringList missingEntries;
        QString classifyError;
        if (!classifyRowsIntoFolder(rows,
                                    trajectoryIndex,
                                    targetFolderPath,
                                    classifyError,
                                    &movedFiles,
                                    &missingFiles,
                                    &missingEntries)) {
            errorMessage = withSheetContext(sheetName, classifyError);
            return false;
        }

        aggregate.movedFiles += movedFiles;
        aggregate.missingFiles += missingFiles;
        aggregate.exportedRows += rows.size();
        aggregate.missingEntries.append(missingEntries);
        ++aggregate.exportedCsvFiles;
        ++processedSheets;
    }

    if (processedSheets == 0) {
        errorMessage = QStringLiteral("没有可归类的有效数据行（需包含车牌和有效日期）");
        return false;
    }

    if (result != nullptr) {
        *result = aggregate;
    }

    return true;
}
