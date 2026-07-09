#include "ParseData/ExcelPreviewModel.h"

#include <QFileInfo>
#include <QVariantMap>

#include "ParseData/BusinessExcelExporter.h"
#include "ParseData/BusinessWorkbookResolver.h"
#include "ParseData/ExcelFilePath.h"
#include "ParseData/LicensePlateDetector.h"
#include "ConfigManager.h"
#include "PostGisTrajectoryImporter.h"

#include <QDir>
#include <QFileInfo>

namespace {

QString formatWorkbookStatus(const ExcelWorkbookInfo& info)
{
    if (info.readerType == ExcelBackend::ReaderType::Xlsxio) {
        return QStringLiteral("共 %1 个工作表（大文件按需预览，每个工作表最多 %2 行 × %3 列）")
            .arg(info.sheetNames.size())
            .arg(info.limits.maxRows)
            .arg(info.limits.maxColumns);
    }
    if (info.readerType == ExcelBackend::ReaderType::OoxmlSax) {
        return QStringLiteral("共 %1 个工作表（Strict OOXML 格式）").arg(info.sheetNames.size());
    }
    return QStringLiteral("共 %1 个工作表").arg(info.sheetNames.size());
}

} // namespace

ExcelPreviewModel::ExcelPreviewModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

bool ExcelPreviewModel::hasData() const
{
    return !m_currentSheet.grid.isEmpty();
}

void ExcelPreviewModel::releaseCurrentSheet()
{
    m_currentSheet.grid.clear();
    m_currentSheet.grid.squeeze();
    m_currentSheet.originalRowNumbers.clear();
    m_currentSheet.isPlateColumn.clear();
    m_currentSheet.isDateColumn.clear();
    m_currentSheet.columnCount = 0;
    m_currentSheet.statusMessage.clear();
}

bool ExcelPreviewModel::isPlateColumn(int tableColumnIndex) const
{
    if (tableColumnIndex <= RowNumberColumn) {
        return false;
    }

    const int dataColumnIndex = tableColumnIndex - 1;
    if (dataColumnIndex < 0 || dataColumnIndex >= m_currentSheet.isPlateColumn.size()) {
        return false;
    }

    return m_currentSheet.isPlateColumn.at(dataColumnIndex);
}

bool ExcelPreviewModel::isDateColumn(int tableColumnIndex) const
{
    if (tableColumnIndex <= RowNumberColumn) {
        return false;
    }

    const int dataColumnIndex = tableColumnIndex - 1;
    if (dataColumnIndex < 0 || dataColumnIndex >= m_currentSheet.isDateColumn.size()) {
        return false;
    }

    return m_currentSheet.isDateColumn.at(dataColumnIndex);
}

namespace {

QVariantList buildColumnOptions(const ExcelSheetPreview& sheet, const QVector<bool>& flags)
{
    QVariantList options;
    for (int columnIndex = 0; columnIndex < flags.size(); ++columnIndex) {
        if (!flags.at(columnIndex)) {
            continue;
        }

        QString sample;
        for (const QVector<QString>& row : sheet.grid) {
            if (columnIndex < row.size()) {
                const QString text = row.at(columnIndex).trimmed();
                if (!text.isEmpty()) {
                    sample = text;
                    break;
                }
            }
        }

        QVariantMap item;
        item.insert(QStringLiteral("columnNumber"), columnIndex + 1);
        item.insert(QStringLiteral("sample"), sample);
        options.append(item);
    }
    return options;
}

} // namespace

QVariantList ExcelPreviewModel::dateColumnOptions() const
{
    return buildColumnOptions(m_currentSheet, m_currentSheet.isDateColumn);
}

QVariantList ExcelPreviewModel::plateColumnOptions() const
{
    return buildColumnOptions(m_currentSheet, m_currentSheet.isPlateColumn);
}

int ExcelPreviewModel::detectedDateColumnCount() const
{
    int count = 0;
    for (bool marked : m_currentSheet.isDateColumn) {
        if (marked) {
            ++count;
        }
    }
    return count;
}

int ExcelPreviewModel::detectedPlateColumnCount() const
{
    return LicensePlateDetector::markedColumnIndices(m_currentSheet).size();
}

int ExcelPreviewModel::defaultPlateColumnNumber() const
{
    const int columnIndex = LicensePlateDetector::firstColumnIndex(m_currentSheet);
    return columnIndex < 0 ? -1 : columnIndex + 1;
}

QString ExcelPreviewModel::suggestedExportFileName() const
{
    const QString sheetName = m_workbookInfo.sheetNames.value(m_currentSheetIndex);
    return BusinessExcelExporter::fileNameForSheet(m_fileName, sheetName, false);
}

QUrl ExcelPreviewModel::suggestedExportFileUrl() const
{
    const QFileInfo excelInfo(m_workbookInfo.filePath);
    const QString exportPath =
        QDir(excelInfo.absolutePath()).filePath(suggestedExportFileName());
    return QUrl::fromLocalFile(exportPath);
}

QUrl ExcelPreviewModel::suggestedExportFolderUrl() const
{
    const QFileInfo excelInfo(m_workbookInfo.filePath);
    return QUrl::fromLocalFile(excelInfo.absolutePath());
}

bool ExcelPreviewModel::exportUsesFolder() const
{
    return BusinessWorkbookResolver::workbookUsesAllSheets(m_workbookInfo);
}

BusinessColumnSelection ExcelPreviewModel::makeColumnSelection(int startColumnNumber,
                                                               int endColumnNumber,
                                                               const QString& singleTimeRole,
                                                               int dayOffset) const
{
    return BusinessColumnSelection::fromUi(
        startColumnNumber, endColumnNumber, singleTimeRole, dayOffset);
}

BusinessColumnSelection ExcelPreviewModel::makeColumnSelection(const QVariantMap& columnConfig) const
{
    return BusinessColumnSelection::fromUi(
        columnConfig.value(QStringLiteral("startColumnNumber")).toInt(),
        columnConfig.value(QStringLiteral("endColumnNumber")).toInt(),
        columnConfig.value(QStringLiteral("singleTimeRole")).toString(),
        columnConfig.value(QStringLiteral("dayOffset")).toInt());
}

bool ExcelPreviewModel::collectBusinessRows(const BusinessColumnSelection& selection,
                                            BusinessWorkbookRowsResult& result,
                                            QString& errorMessage) const
{
    return BusinessWorkbookResolver::collectWorkbookRows(m_workbookInfo,
                                                         m_currentSheet,
                                                         m_currentSheetIndex,
                                                         selection,
                                                         result,
                                                         errorMessage);
}

void ExcelPreviewModel::reportClassifyResult(const BusinessClassifyResult& result)
{
    setErrorMessage(QString());

    QString statusMessage =
        QStringLiteral("归类完成：已移动 %1 个轨迹文件，导出 %2 个 CSV（共 %3 行）")
            .arg(result.movedFiles)
            .arg(result.exportedCsvFiles)
            .arg(result.exportedRows);

    if (result.missingFiles > 0) {
        const int previewCount = qMin(result.missingEntries.size(), 5);
        const QStringList previewEntries = result.missingEntries.mid(0, previewCount);
        statusMessage += QStringLiteral("；未找到 %1 个轨迹文件").arg(result.missingFiles);
        if (!previewEntries.isEmpty()) {
            statusMessage += QStringLiteral("（如 %1").arg(previewEntries.join(QStringLiteral("、")));
            if (result.missingEntries.size() > previewCount) {
                statusMessage += QStringLiteral(" 等");
            }
            statusMessage += QStringLiteral("）");
        }
    }

    if (!result.skippedSheetNames.isEmpty()) {
        statusMessage += QStringLiteral("；跳过 %1 张表：%2")
                             .arg(result.skippedSheetNames.size())
                             .arg(result.skippedSheetNames.join(QStringLiteral("、")));
    }

    setStatusMessage(statusMessage);
    emit statusMessageChanged();
}

bool ExcelPreviewModel::exportBusinessWithConfig(const QString& filePath,
                                                 const QVariantMap& columnConfig)
{
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(filePath);
    if (localPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择导出文件"));
        return false;
    }

    if (!hasData()) {
        setErrorMessage(QStringLiteral("当前没有可导出的数据"));
        return false;
    }

    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);

    BusinessWorkbookRowsResult collected;
    QString errorMessage;
    if (!collectBusinessRows(selection, collected, errorMessage)) {
        setErrorMessage(errorMessage);
        return false;
    }

    int exportedRows = 0;
    if (!BusinessExcelExporter::exportRowsToCsv(collected.sheets.first().rows,
                                                localPath,
                                                collected.sheets.first().sheetName,
                                                errorMessage,
                                                &exportedRows)) {
        setErrorMessage(errorMessage);
        return false;
    }

    setErrorMessage(QString());
    setStatusMessage(QStringLiteral("已导出 %1 行（相同车牌已归并）").arg(exportedRows));
    emit statusMessageChanged();
    return true;
}

bool ExcelPreviewModel::exportBusinessFolderWithConfig(const QString& folderPath,
                                                       const QVariantMap& columnConfig)
{
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择导出目录"));
        return false;
    }

    if (!hasData()) {
        setErrorMessage(QStringLiteral("当前没有可导出的数据"));
        return false;
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
                                                          localPath,
                                                          errorMessage,
                                                          &exportedRows,
                                                          &exportedFiles,
                                                          nullptr,
                                                          &skippedSheets)) {
        setErrorMessage(errorMessage);
        return false;
    }

    setErrorMessage(QString());
    QString statusMessage =
        QStringLiteral("已导出 %1 个 CSV 文件，共 %2 行（每张表一个文件，相同车牌已归并）")
            .arg(exportedFiles)
            .arg(exportedRows);
    if (!skippedSheets.isEmpty()) {
        statusMessage += QStringLiteral("；跳过 %1 张表：%2")
                             .arg(skippedSheets.size())
                             .arg(skippedSheets.join(QStringLiteral("、")));
    }
    setStatusMessage(statusMessage);
    emit statusMessageChanged();
    return true;
}

bool ExcelPreviewModel::classifyWithConfig(const QString& outputFolderPath,
                                           const QString& trajectoryFolderPath,
                                           const QVariantMap& columnConfig)
{
    const QString outputPath = ExcelFilePath::normalizeLocalFilePath(outputFolderPath);
    const QString trajectoryPath = ExcelFilePath::normalizeLocalFilePath(trajectoryFolderPath);
    if (outputPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择输出目录"));
        return false;
    }
    if (trajectoryPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择轨迹文件目录"));
        return false;
    }

    if (!hasData()) {
        setErrorMessage(QStringLiteral("当前没有可归类的数据"));
        return false;
    }

    QString errorMessage;
    BusinessClassifyResult result;
    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);

    if (!BusinessExcelExporter::classifyWorkbookToDirectory(m_workbookInfo,
                                                            m_currentSheet,
                                                            m_currentSheetIndex,
                                                            selection,
                                                            trajectoryPath,
                                                            outputPath,
                                                            errorMessage,
                                                            &result)) {
        setErrorMessage(errorMessage);
        return false;
    }

    reportClassifyResult(result);
    return true;
}

QVariantList ExcelPreviewModel::screenshotTasksWithConfig(const QVariantMap& columnConfig)
{
    QVariantList tasks;
    if (!hasData()) {
        setErrorMessage(QStringLiteral("当前没有可导出的数据"));
        return tasks;
    }

    const BusinessColumnSelection selection = makeColumnSelection(columnConfig);

    BusinessWorkbookRowsResult collected;
    QString errorMessage;
    if (!collectBusinessRows(selection, collected, errorMessage)) {
        setErrorMessage(errorMessage);
        emit errorMessageChanged();
        return tasks;
    }

    const QList<BusinessExportRow> rows = collected.allRowsFlat();
    tasks.reserve(rows.size());
    for (const BusinessExportRow& row : rows) {
        QVariantMap task;
        task.insert(QStringLiteral("plate"), row.plate);
        task.insert(QStringLiteral("startDate"), row.startDate.toString(Qt::ISODate));
        task.insert(QStringLiteral("endDate"), row.endDate.toString(Qt::ISODate));
        tasks.append(task);
    }

    setErrorMessage(QString());
    emit errorMessageChanged();
    return tasks;
}

bool ExcelPreviewModel::importTrajectoryFolderToDatabase(const QString& folderPath)
{
    const QString localPath = ExcelFilePath::normalizeLocalFilePath(folderPath);
    if (localPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择轨迹文件夹"));
        return false;
    }

    setLoading(true);
    setErrorMessage(QString());
    setStatusMessage(QStringLiteral("正在连接数据库并扫描轨迹文件..."));

    PostGisTrajectoryImporter importer;
    connect(&importer, &PostGisTrajectoryImporter::importProgress, this, [this](int progress) {
        setStatusMessage(QStringLiteral("正在导入轨迹到数据库... %1%").arg(progress));
    });

    QString errorMessage;
    TrajectoryImportResult result;
    const PostGisDatabaseConfig config = ConfigManager::GetInstance()->postGisDatabaseConfig();
    const bool success = importer.importFolder(localPath, config, errorMessage, &result);

    setLoading(false);

    if (!success) {
        setErrorMessage(errorMessage);
        return false;
    }

    setErrorMessage(QString());
    QString statusMessage =
        QStringLiteral("导入完成：导入 %1/%2 个文件，跳过 %3 个，新增 %4 个轨迹点")
            .arg(result.importedFiles)
            .arg(result.totalFiles)
            .arg(result.skippedFiles)
            .arg(result.importedPoints);
    if (result.failedFiles > 0) {
        statusMessage += QStringLiteral("；失败 %1 个").arg(result.failedFiles);
        if (!result.errorSamples.isEmpty()) {
            statusMessage += QStringLiteral("（%1").arg(result.errorSamples.first());
            if (result.errorSamples.size() > 1) {
                statusMessage += QStringLiteral(" 等");
            }
            statusMessage += QStringLiteral("）");
        }
    }
    setStatusMessage(statusMessage);
    emit statusMessageChanged();
    return true;
}

bool ExcelPreviewModel::loadSheetAtIndex(int index)
{
    releaseCurrentSheet();

    QString errorMessage;
    ExcelSheetPreview sheet;
    if (!ExcelPreviewLoader::loadSheet(m_workbookInfo, index, sheet, errorMessage)) {
        setErrorMessage(errorMessage);
        beginResetModel();
        m_currentSheetIndex = index;
        endResetModel();
        emit currentSheetIndexChanged();
        emit currentSheetChanged();
        emit hasDataChanged();
        return false;
    }

    beginResetModel();
    m_currentSheet = std::move(sheet);
    m_currentSheetIndex = index;
    endResetModel();

    setErrorMessage(QString());
    emit currentSheetIndexChanged();
    emit currentSheetChanged();
    emit hasDataChanged();
    return true;
}

bool ExcelPreviewModel::loadFile(const QString& filePath)
{
    if (filePath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择文件"));
        emit loadFinished(false);
        return false;
    }

    setLoading(true);
    setErrorMessage(QString());

    const QString localPath = ExcelFilePath::normalizeLocalFilePath(filePath);
    if (localPath.isEmpty()) {
        setErrorMessage(QStringLiteral("未选择文件"));
        setLoading(false);
        emit loadFinished(false);
        return false;
    }

    clear();

    ExcelWorkbookInfo workbookInfo;
    QString errorMessage;
    if (!ExcelPreviewLoader::inspectWorkbook(localPath, workbookInfo, errorMessage)) {
        setErrorMessage(errorMessage);
        setLoading(false);
        emit loadFinished(false);
        return false;
    }

    m_workbookInfo = workbookInfo;
    m_fileName = QFileInfo(localPath).fileName();
    setStatusMessage(formatWorkbookStatus(m_workbookInfo));
    emit filePathChanged();
    emit sheetsChanged();

    const bool sheetLoaded = loadSheetAtIndex(0);
    setLoading(false);
    emit loadFinished(sheetLoaded);
    return sheetLoaded;
}

void ExcelPreviewModel::clear()
{
    beginResetModel();
    releaseCurrentSheet();
    m_workbookInfo = ExcelWorkbookInfo{};
    m_currentSheetIndex = -1;
    m_fileName.clear();
    endResetModel();

    setStatusMessage(QString());
    setErrorMessage(QString());
    setLoading(false);

    emit filePathChanged();
    emit sheetsChanged();
    emit currentSheetIndexChanged();
    emit currentSheetChanged();
    emit hasDataChanged();
}

void ExcelPreviewModel::setCurrentSheetIndex(int index)
{
    if (index < 0 || index >= m_workbookInfo.sheetNames.size() || index == m_currentSheetIndex) {
        return;
    }

    setLoading(true);
    const bool loaded = loadSheetAtIndex(index);
    setLoading(false);

    if (!loaded) {
        emit loadFinished(false);
    }
}

int ExcelPreviewModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_currentSheet.grid.size();
}

int ExcelPreviewModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return previewColumnCount();
}

QVariant ExcelPreviewModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) {
        return QVariant();
    }

    const int row = index.row();
    const int column = index.column();
    if (row < 0 || row >= m_currentSheet.grid.size()) {
        return QVariant();
    }

    if (column == RowNumberColumn) {
        if (row < m_currentSheet.originalRowNumbers.size()) {
            return m_currentSheet.originalRowNumbers.at(row);
        }
        return row + 1;
    }

    const int dataColumn = column - 1;
    if (dataColumn < 0 || dataColumn >= m_currentSheet.columnCount) {
        return QVariant();
    }

    return m_currentSheet.grid.at(row).value(dataColumn);
}

QVariant ExcelPreviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        if (section == RowNumberColumn) {
            return QStringLiteral("行");
        }
        return QStringLiteral("列 %1").arg(section);
    }

    if (section >= 0 && section < m_currentSheet.originalRowNumbers.size()) {
        return m_currentSheet.originalRowNumbers.at(section);
    }

    return section + 1;
}

QHash<int, QByteArray> ExcelPreviewModel::roleNames() const
{
    return {{Qt::DisplayRole, "display"}};
}

void ExcelPreviewModel::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    emit loadingChanged();
}

void ExcelPreviewModel::setErrorMessage(const QString& message)
{
    if (m_errorMessage == message) {
        return;
    }
    m_errorMessage = message;
    emit errorMessageChanged();
}

void ExcelPreviewModel::setStatusMessage(const QString& message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}


