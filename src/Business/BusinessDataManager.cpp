#include "Business/BusinessDataManager.h"

#include "Business/BusinessColumnIdentifier.h"
#include "Business/BusinessExcelExporter.h"
#include "Business/BusinessWorkbookResolver.h"
#include "DataManagement/TrajectoryImportManager.h"
#include "ExcelDriver/ExcelParserManager.h"
#include "Core/LocalFilePath.h"

#include <QDir>
#include <QFileInfo>

BusinessDataManager::BusinessDataManager(QObject* parent)
    : QObject(parent)
    , m_importManager(new TrajectoryImportManager(this))
    , m_excelParser(new ExcelParserManager(this))
{
    connect(m_importManager, &TrajectoryImportManager::importProgress,
            this, &BusinessDataManager::importProgress);
}

BusinessColumnSelection BusinessDataManager::makeColumnSelection(const QVariantMap& columnConfig) const
{
    return BusinessColumnSelection::fromUi(
        columnConfig.value(QStringLiteral("startColumnNumber")).toInt(),
        columnConfig.value(QStringLiteral("endColumnNumber")).toInt(),
        columnConfig.value(QStringLiteral("singleTimeRole")).toString(),
        columnConfig.value(QStringLiteral("dayOffset")).toInt());
}

bool BusinessDataManager::collectBusinessRows(const BusinessColumnSelection& selection,
                                              BusinessWorkbookRowsResult& result,
                                              QString& errorMessage) const
{
    return BusinessWorkbookResolver::collectWorkbookRows(m_workbookInfo,
                                                         m_currentSheet,
                                                         m_currentSheetIndex,
                                                         selection,
                                                         *m_excelParser,
                                                         result,
                                                         errorMessage);
}

BusinessDataManager::OperationResult BusinessDataManager::makeClassifyResult(
    const BusinessClassifyResult& result)
{
    OperationResult op;
    op.success = true;
    op.statusMessage =
        QStringLiteral("归类完成：已移动 %1 个轨迹文件，导出 %2 个 CSV（共 %3 行）")
            .arg(result.movedFiles)
            .arg(result.exportedCsvFiles)
            .arg(result.exportedRows);

    if (result.missingFiles > 0) {
        const int previewCount = qMin(result.missingEntries.size(), 5);
        const QStringList previewEntries = result.missingEntries.mid(0, previewCount);
        op.statusMessage += QStringLiteral("；未找到 %1 个轨迹文件").arg(result.missingFiles);
        if (!previewEntries.isEmpty()) {
            op.statusMessage += QStringLiteral("（如 %1").arg(previewEntries.join(QStringLiteral("、")));
            if (result.missingEntries.size() > previewCount) {
                op.statusMessage += QStringLiteral(" 等");
            }
            op.statusMessage += QStringLiteral("）");
        }
    }

    if (!result.skippedSheetNames.isEmpty()) {
        op.statusMessage += QStringLiteral("；跳过 %1 张表：%2")
                                .arg(result.skippedSheetNames.size())
                                .arg(result.skippedSheetNames.join(QStringLiteral("、")));
    }
    return op;
}

void BusinessDataManager::clearWorkbook()
{
    m_workbookInfo = ExcelWorkbookInfo{};
    m_currentSheet = ExcelSheetPreview{};
    m_currentSheetIndex = -1;
    m_fileName.clear();
    m_workbookStatusMessage.clear();
    cancelScreenshotTasks();
    emit workbookCleared();
}

BusinessDataManager::OperationResult BusinessDataManager::openWorkbook(const QString& filePath)
{
    OperationResult op;
    if (filePath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择文件");
        return op;
    }

    const QString localPath = LocalFilePath::normalizeLocalFilePath(filePath);
    if (localPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择文件");
        return op;
    }

    clearWorkbook();

    QString errorMessage;
    if (!m_excelParser->inspectWorkbook(localPath, m_workbookInfo, errorMessage)) {
        op.errorMessage = errorMessage;
        return op;
    }

    m_fileName = QFileInfo(localPath).fileName();
    m_workbookStatusMessage = ExcelParserManager::formatWorkbookStatus(m_workbookInfo);

    op = loadSheetAtIndex(0);
    if (op.success) {
        op.statusMessage = m_workbookStatusMessage;
    }
    return op;
}

BusinessDataManager::OperationResult BusinessDataManager::loadSheetAtIndex(int index)
{
    OperationResult op;

    m_currentSheet = ExcelSheetPreview{};
    QString errorMessage;
    ExcelSheetPreview sheet;
    if (!m_excelParser->loadSheet(m_workbookInfo, index, sheet, errorMessage)) {
        op.errorMessage = errorMessage;
        m_currentSheetIndex = index;
        emit sheetChanged();
        return op;
    }

    BusinessColumnIdentifier::identifyColumns(sheet);
    BusinessColumnIdentifier::appendColumnStatus(sheet);

    m_currentSheet = std::move(sheet);
    m_currentSheetIndex = index;
    op.success = true;
    emit sheetChanged();
    return op;
}

BusinessDataManager::OperationResult BusinessDataManager::exportToFile(const QString& filePath,
                                                                       const QVariantMap& columnConfig)
{
    OperationResult op;
    const QString localPath = LocalFilePath::normalizeLocalFilePath(filePath);
    if (localPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择导出文件");
        return op;
    }
    if (!hasWorkbook()) {
        op.errorMessage = QStringLiteral("当前没有可导出的数据");
        return op;
    }

    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);
    BusinessWorkbookRowsResult collected;
    QString errorMessage;
    if (!collectBusinessRows(selection, collected, errorMessage)) {
        op.errorMessage = errorMessage;
        return op;
    }

    int exportedRows = 0;
    if (!BusinessExcelExporter::exportRowsToCsv(collected.sheets.first().rows,
                                                localPath,
                                                collected.sheets.first().sheetName,
                                                errorMessage,
                                                &exportedRows)) {
        op.errorMessage = errorMessage;
        return op;
    }

    op.success = true;
    op.statusMessage = QStringLiteral("已导出 %1 行（相同车牌已归并）").arg(exportedRows);
    return op;
}

BusinessDataManager::OperationResult BusinessDataManager::exportToFolder(const QString& folderPath,
                                                                         const QVariantMap& columnConfig)
{
    OperationResult op;
    const QString localPath = LocalFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择导出目录");
        return op;
    }
    if (!hasWorkbook()) {
        op.errorMessage = QStringLiteral("当前没有可导出的数据");
        return op;
    }

    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);
    QString errorMessage;
    int exportedRows = 0;
    int exportedFiles = 0;
    QStringList skippedSheets;
    if (!BusinessExcelExporter::exportWorkbookToDirectory(m_workbookInfo,
                                                          m_currentSheet,
                                                          m_currentSheetIndex,
                                                          selection,
                                                          *m_excelParser,
                                                          localPath,
                                                          errorMessage,
                                                          &exportedRows,
                                                          &exportedFiles,
                                                          nullptr,
                                                          &skippedSheets)) {
        op.errorMessage = errorMessage;
        return op;
    }

    op.success = true;
    op.statusMessage =
        QStringLiteral("已导出 %1 个 CSV 文件，共 %2 行（每张表一个文件，相同车牌已归并）")
            .arg(exportedFiles)
            .arg(exportedRows);
    if (!skippedSheets.isEmpty()) {
        op.statusMessage += QStringLiteral("；跳过 %1 张表：%2")
                                .arg(skippedSheets.size())
                                .arg(skippedSheets.join(QStringLiteral("、")));
    }
    return op;
}

BusinessDataManager::OperationResult BusinessDataManager::classifyToFolder(
    const QString& outputFolderPath,
    const QString& trajectoryFolderPath,
    const QVariantMap& columnConfig)
{
    OperationResult op;
    const QString outputPath = LocalFilePath::normalizeLocalFilePath(outputFolderPath);
    const QString trajectoryPath = LocalFilePath::normalizeLocalFilePath(trajectoryFolderPath);
    if (outputPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择输出目录");
        return op;
    }
    if (trajectoryPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择轨迹文件目录");
        return op;
    }
    if (!hasWorkbook()) {
        op.errorMessage = QStringLiteral("当前没有可归类的数据");
        return op;
    }

    QString errorMessage;
    BusinessClassifyResult result;
    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);
    if (!BusinessExcelExporter::classifyWorkbookToDirectory(m_workbookInfo,
                                                            m_currentSheet,
                                                            m_currentSheetIndex,
                                                            selection,
                                                            *m_excelParser,
                                                            trajectoryPath,
                                                            outputPath,
                                                            errorMessage,
                                                            &result)) {
        op.errorMessage = errorMessage;
        return op;
    }

    return makeClassifyResult(result);
}

BusinessDataManager::OperationResult BusinessDataManager::importTrajectoryFolder(const QString& folderPath)
{
    OperationResult op;
    const QString localPath = LocalFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        op.errorMessage = QStringLiteral("未选择轨迹文件夹");
        return op;
    }

    QString statusMessage;
    if (!m_importManager->importFolder(localPath, &statusMessage)) {
        op.errorMessage = statusMessage;
        return op;
    }

    op.success = true;
    op.statusMessage = statusMessage;
    return op;
}

BusinessDataManager::OperationResult BusinessDataManager::beginScreenshotTasks(const QVariantMap& columnConfig)
{
    OperationResult op;
    cancelScreenshotTasks();

    if (!hasWorkbook()) {
        op.errorMessage = QStringLiteral("当前没有可导出的数据");
        return op;
    }

    m_screenshotSelection = makeColumnSelection(columnConfig);
    m_screenshotSheetIndices =
        BusinessWorkbookResolver::processableSheetIndices(m_workbookInfo, m_currentSheetIndex);
    if (m_screenshotSheetIndices.isEmpty()) {
        op.errorMessage = QStringLiteral("工作簿中没有可处理的工作表");
        return op;
    }

    m_screenshotIterActive = true;
    m_screenshotNextSheetListIndex = 0;
    m_screenshotPendingRows.clear();
    m_screenshotNextRowIndex = 0;

    QString errorMessage;
    if (!advanceScreenshotSheet(errorMessage)) {
        m_screenshotIterActive = false;
        op.errorMessage = errorMessage;
        return op;
    }

    if (m_screenshotPendingRows.isEmpty()) {
        m_screenshotIterActive = false;
        op.errorMessage = QStringLiteral("没有有效的业务数据行（需包含车牌和有效日期）");
        return op;
    }

    op.success = true;
    return op;
}

QVariantMap BusinessDataManager::nextScreenshotTask()
{
    if (!m_screenshotIterActive) {
        return {};
    }

    while (m_screenshotNextRowIndex >= m_screenshotPendingRows.size()) {
        m_screenshotPendingRows.clear();
        m_screenshotNextRowIndex = 0;

        if (m_screenshotNextSheetListIndex >= m_screenshotSheetIndices.size()) {
            cancelScreenshotTasks();
            return {};
        }

        QString errorMessage;
        if (!advanceScreenshotSheet(errorMessage)) {
            cancelScreenshotTasks();
            return {};
        }
    }

    const BusinessExportRow& row = m_screenshotPendingRows.at(m_screenshotNextRowIndex++);
    QVariantMap task;
    task.insert(QStringLiteral("plate"), row.plate);
    task.insert(QStringLiteral("startDate"), row.startDate.toString(Qt::ISODate));
    task.insert(QStringLiteral("endDate"), row.endDate.toString(Qt::ISODate));
    return task;
}

void BusinessDataManager::cancelScreenshotTasks()
{
    m_screenshotIterActive = false;
    m_screenshotSheetIndices.clear();
    m_screenshotNextSheetListIndex = 0;
    m_screenshotPendingRows.clear();
    m_screenshotNextRowIndex = 0;
}

bool BusinessDataManager::advanceScreenshotSheet(QString& errorMessage)
{
    errorMessage.clear();

    while (m_screenshotNextSheetListIndex < m_screenshotSheetIndices.size()) {
        const int sheetIndex = m_screenshotSheetIndices.at(m_screenshotNextSheetListIndex);
        BusinessSheetRows sheetRows;
        bool sheetSkipped = false;
        if (!BusinessWorkbookResolver::collectSheetBusinessRows(m_workbookInfo,
                                                                m_currentSheet,
                                                                sheetIndex,
                                                                m_screenshotSelection,
                                                                *m_excelParser,
                                                                sheetRows,
                                                                errorMessage,
                                                                &sheetSkipped)) {
            return false;
        }

        ++m_screenshotNextSheetListIndex;
        if (sheetSkipped || sheetRows.rows.isEmpty()) {
            continue;
        }

        m_screenshotPendingRows = sheetRows.rows;
        m_screenshotNextRowIndex = 0;
        return true;
    }

    return true;
}

QUrl BusinessDataManager::suggestedExportFileUrl() const
{
    const QString sheetName = m_workbookInfo.sheetNames.value(m_currentSheetIndex);
    const QString exportName = BusinessExcelExporter::fileNameForSheet(m_fileName, sheetName, false);
    const QFileInfo excelInfo(m_workbookInfo.filePath);
    return QUrl::fromLocalFile(QDir(excelInfo.absolutePath()).filePath(exportName));
}

QUrl BusinessDataManager::suggestedExportFolderUrl() const
{
    return QUrl::fromLocalFile(QFileInfo(m_workbookInfo.filePath).absolutePath());
}

bool BusinessDataManager::exportUsesFolder() const
{
    return BusinessWorkbookResolver::workbookUsesAllSheets(m_workbookInfo);
}
