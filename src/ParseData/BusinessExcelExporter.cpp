#include "ParseData/BusinessExcelExporter.h"

#include "ParseData/BusinessCsvWriter.h"
#include "ParseData/BusinessWorkbookResolver.h"
#include "ParseData/ExcelFilePath.h"
#include "ParseData/TrajectoryFileNaming.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

namespace {

bool classifyOneSheet(const BusinessSheetRows& sheetRows,
                      const QHash<QString, QString>& trajectoryIndex,
                      const QDir& outputDir,
                      const QString& excelFileName,
                      bool appendSheetName,
                      QString& errorMessage,
                      int* movedFiles,
                      int* missingFiles,
                      QStringList* missingEntries)
{
    const QString csvFileName = BusinessExcelExporter::fileNameForSheet(excelFileName,
                                                                        sheetRows.sheetName,
                                                                        appendSheetName);
    const QString csvFilePath = outputDir.filePath(csvFileName);

    QString writeError;
    if (!BusinessCsvWriter::writeRowsToCsv(sheetRows.rows, csvFilePath, writeError)) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(sheetRows.sheetName, writeError);
        return false;
    }

    const QString folderName =
        BusinessExcelExporter::folderNameForSheet(excelFileName, sheetRows.sheetName, appendSheetName);
    const QString targetFolderPath = outputDir.filePath(folderName);

    QString classifyError;
    if (!BusinessExcelExporter::classifyRowsIntoFolder(sheetRows.rows,
                                                       trajectoryIndex,
                                                       targetFolderPath,
                                                       classifyError,
                                                       movedFiles,
                                                       missingFiles,
                                                       missingEntries)) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(sheetRows.sheetName, classifyError);
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
    QString baseName = fileInfo.completeBaseName();
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("export");
    }

    if (!appendSheetName || sheetName.trimmed().isEmpty()) {
        return baseName + QStringLiteral(".csv");
    }

    const QString sanitizedSheetName = ExcelFilePath::sanitizeFileComponent(sheetName);
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

bool BusinessExcelExporter::exportRowsToCsv(const QList<BusinessExportRow>& rows,
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

    if (!BusinessCsvWriter::writeRowsToCsv(rows, filePath, errorMessage)) {
        errorMessage = BusinessWorkbookResolver::formatSheetError(sheetName, errorMessage);
        return false;
    }

    if (exportedRows != nullptr) {
        *exportedRows = rows.size();
    }

    return true;
}

bool BusinessExcelExporter::exportWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                                      const ExcelSheetPreview& referenceSheet,
                                                      int currentSheetIndex,
                                                      const BusinessColumnSelection& selection,
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
                                                       collected,
                                                       errorMessage)) {
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
        if (!exportRowsToCsv(sheetRows.rows, filePath, sheetRows.sheetName, writeError, &sheetExportedRows)) {
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

    return true;
}

bool BusinessExcelExporter::classifyRowsIntoFolder(const QList<BusinessExportRow>& rows,
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
        const QString key =
            TrajectoryFileNaming::lookupKey(row.plate, row.startDate, row.endDate);
        const auto indexIt = trajectoryIndex.constFind(key);
        if (indexIt == trajectoryIndex.constEnd()) {
            ++missing;
            if (missingEntries != nullptr) {
                missingEntries->append(
                    TrajectoryFileNaming::fileBaseName(row.plate, row.startDate, row.endDate)
                    + QStringLiteral(".xlsx"));
            }
            continue;
        }

        const QFileInfo sourceInfo(indexIt.value());
        const QString destinationPath = targetDir.filePath(sourceInfo.fileName());

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

bool BusinessExcelExporter::moveTrajectoryFile(const QString& sourcePath,
                                               const QString& destinationPath,
                                               QString& errorMessage)
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

bool BusinessExcelExporter::classifyWorkbookToDirectory(
    const ExcelWorkbookInfo& workbookInfo,
    const ExcelSheetPreview& referenceSheet,
    int currentSheetIndex,
    const BusinessColumnSelection& selection,
    const QString& trajectoryDirectory,
    const QString& outputDirectory,
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

    BusinessWorkbookRowsResult collected;
    if (!BusinessWorkbookResolver::collectWorkbookRows(workbookInfo,
                                                       referenceSheet,
                                                       currentSheetIndex,
                                                       selection,
                                                       collected,
                                                       errorMessage)) {
        return false;
    }

    const QString excelFileName = QFileInfo(workbookInfo.filePath).fileName();
    const bool appendSheetName = BusinessWorkbookResolver::workbookUsesAllSheets(workbookInfo);

    QString trajectoryIndexError;
    const QHash<QString, QString> trajectoryIndex =
        TrajectoryFileNaming::indexRangeFiles(trajectoryDirectory, trajectoryIndexError);
    if (!trajectoryIndexError.isEmpty()) {
        errorMessage = trajectoryIndexError;
        return false;
    }

    if (trajectoryIndex.isEmpty()) {
        errorMessage = QStringLiteral("轨迹目录中没有找到符合命名规则的文件（车牌-开始日期-结束日期.xlsx）");
        return false;
    }

    const QDir outputDir(outputDirectory);
    BusinessClassifyResult aggregate;
    aggregate.skippedSheetNames = collected.skippedSheetNames;

    for (const BusinessSheetRows& sheetRows : collected.sheets) {
        int movedFiles = 0;
        int missingFiles = 0;
        QStringList missingEntries;
        if (!classifyOneSheet(sheetRows,
                              trajectoryIndex,
                              outputDir,
                              excelFileName,
                              appendSheetName,
                              errorMessage,
                              &movedFiles,
                              &missingFiles,
                              &missingEntries)) {
            return false;
        }

        aggregate.movedFiles += movedFiles;
        aggregate.missingFiles += missingFiles;
        aggregate.exportedRows += sheetRows.rows.size();
        aggregate.missingEntries.append(missingEntries);
        ++aggregate.exportedCsvFiles;
    }

    if (result != nullptr) {
        *result = aggregate;
    }

    return true;
}
