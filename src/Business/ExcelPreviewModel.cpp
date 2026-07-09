#include "Business/ExcelPreviewModel.h"

#include "Business/BusinessExcelExporter.h"
#include "Business/LicensePlateDetector.h"

#include <QFileInfo>

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

ExcelPreviewModel::ExcelPreviewModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_dataManager(new BusinessDataManager(this))
{
    connect(m_dataManager, &BusinessDataManager::sheetChanged, this, &ExcelPreviewModel::onBusinessSheetChanged);
    connect(m_dataManager, &BusinessDataManager::workbookCleared, this, &ExcelPreviewModel::onBusinessWorkbookCleared);
    connect(m_dataManager, &BusinessDataManager::importProgress, this, &ExcelPreviewModel::onImportProgress);
}

QString ExcelPreviewModel::filePath() const
{
    return m_dataManager->filePath();
}

QString ExcelPreviewModel::fileName() const
{
    return m_dataManager->fileName();
}

QString ExcelPreviewModel::currentSheetStatus() const
{
    return m_dataManager->currentSheet().statusMessage;
}

bool ExcelPreviewModel::hasData() const
{
    return m_dataManager->hasWorkbook();
}

QStringList ExcelPreviewModel::sheetNames() const
{
    return m_dataManager->workbookInfo().sheetNames;
}

int ExcelPreviewModel::sheetCount() const
{
    return m_dataManager->workbookInfo().sheetNames.size();
}

int ExcelPreviewModel::currentSheetIndex() const
{
    return m_dataManager->currentSheetIndex();
}

int ExcelPreviewModel::previewRowCount() const
{
    return m_dataManager->currentSheet().grid.size();
}

int ExcelPreviewModel::previewColumnCount() const
{
    return m_dataManager->currentSheet().columnCount > 0
               ? m_dataManager->currentSheet().columnCount + 1
               : 0;
}

int ExcelPreviewModel::previewDataColumnCount() const
{
    return m_dataManager->currentSheet().columnCount;
}

bool ExcelPreviewModel::isPlateColumn(int tableColumnIndex) const
{
    if (tableColumnIndex <= RowNumberColumn) {
        return false;
    }
    const int dataColumnIndex = tableColumnIndex - 1;
    const ExcelSheetPreview& sheet = m_dataManager->currentSheet();
    if (dataColumnIndex < 0 || dataColumnIndex >= sheet.isPlateColumn.size()) {
        return false;
    }
    return sheet.isPlateColumn.at(dataColumnIndex);
}

bool ExcelPreviewModel::isDateColumn(int tableColumnIndex) const
{
    if (tableColumnIndex <= RowNumberColumn) {
        return false;
    }
    const int dataColumnIndex = tableColumnIndex - 1;
    const ExcelSheetPreview& sheet = m_dataManager->currentSheet();
    if (dataColumnIndex < 0 || dataColumnIndex >= sheet.isDateColumn.size()) {
        return false;
    }
    return sheet.isDateColumn.at(dataColumnIndex);
}

QVariantList ExcelPreviewModel::dateColumnOptions() const
{
    return buildColumnOptions(m_dataManager->currentSheet(), m_dataManager->currentSheet().isDateColumn);
}

QVariantList ExcelPreviewModel::plateColumnOptions() const
{
    return buildColumnOptions(m_dataManager->currentSheet(), m_dataManager->currentSheet().isPlateColumn);
}

int ExcelPreviewModel::detectedDateColumnCount() const
{
    int count = 0;
    for (bool marked : m_dataManager->currentSheet().isDateColumn) {
        if (marked) {
            ++count;
        }
    }
    return count;
}

int ExcelPreviewModel::detectedPlateColumnCount() const
{
    return LicensePlateDetector::markedColumnIndices(m_dataManager->currentSheet()).size();
}

int ExcelPreviewModel::defaultPlateColumnNumber() const
{
    const int columnIndex = LicensePlateDetector::firstColumnIndex(m_dataManager->currentSheet());
    return columnIndex < 0 ? -1 : columnIndex + 1;
}

QString ExcelPreviewModel::suggestedExportFileName() const
{
    const QString sheetName = m_dataManager->workbookInfo().sheetNames.value(currentSheetIndex());
    return BusinessExcelExporter::fileNameForSheet(fileName(), sheetName, false);
}

QUrl ExcelPreviewModel::suggestedExportFileUrl() const
{
    return m_dataManager->suggestedExportFileUrl();
}

QUrl ExcelPreviewModel::suggestedExportFolderUrl() const
{
    return m_dataManager->suggestedExportFolderUrl();
}

bool ExcelPreviewModel::exportUsesFolder() const
{
    return m_dataManager->exportUsesFolder();
}

void ExcelPreviewModel::applyOperationResult(const BusinessDataManager::OperationResult& result,
                                             bool updateStatus)
{
    if (!result.errorMessage.isEmpty()) {
        setErrorMessage(result.errorMessage);
    } else {
        setErrorMessage(QString());
    }
    if (updateStatus && !result.statusMessage.isEmpty()) {
        setStatusMessage(result.statusMessage);
    }
}

void ExcelPreviewModel::resetModelFromBusinessData()
{
    beginResetModel();
    endResetModel();
    emit filePathChanged();
    emit sheetsChanged();
    emit currentSheetIndexChanged();
    emit currentSheetChanged();
    emit hasDataChanged();
}

void ExcelPreviewModel::onBusinessSheetChanged()
{
    resetModelFromBusinessData();
}

void ExcelPreviewModel::onBusinessWorkbookCleared()
{
    resetModelFromBusinessData();
}

void ExcelPreviewModel::onImportProgress(int percentage)
{
    setStatusMessage(QStringLiteral("正在导入轨迹到数据库... %1%").arg(percentage));
}

bool ExcelPreviewModel::loadFile(const QString& filePath)
{
    setLoading(true);
    setErrorMessage(QString());

    const BusinessDataManager::OperationResult result = m_dataManager->openWorkbook(filePath);
    setLoading(false);

    if (!result.success) {
        applyOperationResult(result, false);
        resetModelFromBusinessData();
        emit loadFinished(false);
        return false;
    }

    applyOperationResult(result);
    resetModelFromBusinessData();
    emit loadFinished(true);
    return true;
}

void ExcelPreviewModel::clear()
{
    m_dataManager->clearWorkbook();
    setStatusMessage(QString());
    setErrorMessage(QString());
    setLoading(false);
}

void ExcelPreviewModel::setCurrentSheetIndex(int index)
{
    if (index < 0 || index >= sheetCount() || index == currentSheetIndex()) {
        return;
    }

    setLoading(true);
    const BusinessDataManager::OperationResult result = m_dataManager->loadSheetAtIndex(index);
    setLoading(false);

    if (!result.success) {
        applyOperationResult(result, false);
        emit loadFinished(false);
        return;
    }

    setErrorMessage(QString());
    emit currentSheetIndexChanged();
}

bool ExcelPreviewModel::exportBusinessWithConfig(const QString& filePath, const QVariantMap& columnConfig)
{
    const BusinessDataManager::OperationResult result = m_dataManager->exportToFile(filePath, columnConfig);
    applyOperationResult(result);
    return result.success;
}

bool ExcelPreviewModel::exportBusinessFolderWithConfig(const QString& folderPath,
                                                       const QVariantMap& columnConfig)
{
    const BusinessDataManager::OperationResult result = m_dataManager->exportToFolder(folderPath, columnConfig);
    applyOperationResult(result);
    return result.success;
}

bool ExcelPreviewModel::classifyWithConfig(const QString& outputFolderPath,
                                           const QString& trajectoryFolderPath,
                                           const QVariantMap& columnConfig)
{
    const BusinessDataManager::OperationResult result =
        m_dataManager->classifyToFolder(outputFolderPath, trajectoryFolderPath, columnConfig);
    applyOperationResult(result);
    return result.success;
}

bool ExcelPreviewModel::beginScreenshotTasks(const QVariantMap& columnConfig)
{
    const BusinessDataManager::OperationResult result = m_dataManager->beginScreenshotTasks(columnConfig);
    applyOperationResult(result, false);
    return result.success;
}

QVariantMap ExcelPreviewModel::nextScreenshotTask()
{
    return m_dataManager->nextScreenshotTask();
}

void ExcelPreviewModel::cancelScreenshotTasks()
{
    m_dataManager->cancelScreenshotTasks();
}

bool ExcelPreviewModel::importTrajectoryFolderToDatabase(const QString& folderPath)
{
    setLoading(true);
    setErrorMessage(QString());
    setStatusMessage(QStringLiteral("正在连接数据库并扫描轨迹文件..."));

    const BusinessDataManager::OperationResult result = m_dataManager->importTrajectoryFolder(folderPath);
    setLoading(false);
    applyOperationResult(result);
    return result.success;
}

int ExcelPreviewModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_dataManager->currentSheet().grid.size();
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

    const ExcelSheetPreview& sheet = m_dataManager->currentSheet();
    const int row = index.row();
    const int column = index.column();
    if (row < 0 || row >= sheet.grid.size()) {
        return QVariant();
    }

    if (column == RowNumberColumn) {
        if (row < sheet.originalRowNumbers.size()) {
            return sheet.originalRowNumbers.at(row);
        }
        return row + 1;
    }

    const int dataColumn = column - 1;
    if (dataColumn < 0 || dataColumn >= sheet.columnCount) {
        return QVariant();
    }

    return sheet.grid.at(row).value(dataColumn);
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

        QString label = QStringLiteral("列 %1").arg(section);
        if (isPlateColumn(section)) {
            label += QStringLiteral(" [车牌]");
        } else if (isDateColumn(section)) {
            label += QStringLiteral(" [时间]");
        }
        return label;
    }

    const ExcelSheetPreview& sheet = m_dataManager->currentSheet();
    if (section >= 0 && section < sheet.originalRowNumbers.size()) {
        return sheet.originalRowNumbers.at(section);
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
