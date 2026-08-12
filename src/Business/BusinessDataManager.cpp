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
                                                         errorMessage,
                                                         true);
}

BusinessDataManager::OpenWorkbookResult BusinessDataManager::openWorkbookBlocking(const QString& filePath)
{
    OpenWorkbookResult result;
    if (filePath.isEmpty()) {
        result.op.errorMessage = QStringLiteral("未选择文件");
        return result;
    }

    const QString localPath = LocalFilePath::normalizeLocalFilePath(filePath);
    if (localPath.isEmpty()) {
        result.op.errorMessage = QStringLiteral("未选择文件");
        return result;
    }

    ExcelParserManager parser;
    QString errorMessage;
    if (!parser.inspectWorkbook(localPath, result.workbookInfo, errorMessage)) {
        result.op.errorMessage = errorMessage;
        return result;
    }

    ExcelSheetPreview sheet;
    if (!parser.loadSheet(result.workbookInfo, 0, sheet, errorMessage)) {
        result.op.errorMessage = errorMessage;
        result.sheetIndex = 0;
        return result;
    }

    BusinessColumnIdentifier::identifyColumns(sheet);
    BusinessColumnIdentifier::appendColumnStatus(sheet);

    result.fileName = QFileInfo(localPath).fileName();
    result.workbookStatusMessage = ExcelParserManager::formatWorkbookStatus(result.workbookInfo);
    result.sheet = std::move(sheet);
    result.sheetIndex = 0;
    result.op.success = true;
    result.op.statusMessage = result.workbookStatusMessage;
    return result;
}

BusinessDataManager::SheetLoadResult BusinessDataManager::loadSheetBlocking(
    const ExcelWorkbookInfo& workbookInfo,
    int index)
{
    SheetLoadResult result;
    result.sheetIndex = index;

    ExcelParserManager parser;
    QString errorMessage;
    ExcelSheetPreview sheet;
    if (!parser.loadSheet(workbookInfo, index, sheet, errorMessage)) {
        result.op.errorMessage = errorMessage;
        return result;
    }

    BusinessColumnIdentifier::identifyColumns(sheet);
    BusinessColumnIdentifier::appendColumnStatus(sheet);

    result.sheet = std::move(sheet);
    result.op.success = true;
    return result;
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
    clearWorkbook();

    const OpenWorkbookResult loaded = openWorkbookBlocking(filePath);
    OperationResult op = loaded.op;
    if (!op.success) {
        return op;
    }

    applyOpenWorkbookResult(loaded);
    return op;
}

void BusinessDataManager::applyOpenWorkbookResult(const OpenWorkbookResult& result)
{
    m_workbookInfo = result.workbookInfo;
    m_currentSheet = result.sheet;
    m_currentSheetIndex = result.sheetIndex;
    m_fileName = result.fileName;
    m_workbookStatusMessage = result.workbookStatusMessage;
    emit sheetChanged();
}

void BusinessDataManager::applySheetLoadResult(const SheetLoadResult& result)
{
    m_currentSheet = result.sheet;
    m_currentSheetIndex = result.sheetIndex;
    emit sheetChanged();
}

BusinessDataManager::OperationResult BusinessDataManager::loadSheetAtIndex(int index)
{
    OperationResult op;

    const SheetLoadResult loaded = loadSheetBlocking(m_workbookInfo, index);
    if (!loaded.op.success) {
        op = loaded.op;
        m_currentSheetIndex = index;
        emit sheetChanged();
        return op;
    }

    applySheetLoadResult(loaded);
    op.success = true;
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
    if (!BusinessExcelExporter::exportRowsToXlsx(collected.sheets.first().rows,
                                                 localPath,
                                                 collected.sheets.first().sheetName,
                                                 errorMessage,
                                                 &exportedRows)) {
        op.errorMessage = errorMessage;
        return op;
    }

    QString reportPath;
    if (!BusinessExcelExporter::writeAnomalyReport(collected.anomalyMessages,
                                                   m_workbookInfo.filePath,
                                                   QFileInfo(localPath).absolutePath(),
                                                   errorMessage,
                                                   &reportPath)) {
        op.errorMessage = errorMessage;
        return op;
    }

    op.success = true;
    op.statusMessage = QStringLiteral("已导出 %1 行，并生成异常说明：%2")
                           .arg(exportedRows)
                           .arg(QFileInfo(reportPath).fileName());
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
        QStringLiteral("已导出 %1 个 XLSX 文件，共 %2 行（重复记录已写入异常说明，不导出）")
            .arg(exportedFiles)
            .arg(exportedRows);
    op.statusMessage += QStringLiteral("；已生成与源 Excel 同名的异常说明 TXT");
    if (!skippedSheets.isEmpty()) {
        op.statusMessage += QStringLiteral("；跳过 %1 张表：%2")
                                .arg(skippedSheets.size())
                                .arg(skippedSheets.join(QStringLiteral("、")));
    }
    return op;
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
