#ifndef BUSINESSDATAMANAGER_H
#define BUSINESSDATAMANAGER_H

#include "Business/BusinessWorkbookTypes.h"
#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class ExcelParserManager;
class TrajectoryImportManager;

/**
 * @brief 业务数据管理器：统一 Excel 预览、列识别、导出/归类/截图/导入，隐藏 ExcelDriver 与 Importer 细节。
 */
class BusinessDataManager : public QObject
{
    Q_OBJECT

public:
    struct OperationResult {
        bool success = false;
        QString errorMessage;
        QString statusMessage;
    };

    explicit BusinessDataManager(QObject* parent = nullptr);

    QString filePath() const { return m_workbookInfo.filePath; }
    QString fileName() const { return m_fileName; }
    QString workbookStatusMessage() const { return m_workbookStatusMessage; }
    const ExcelWorkbookInfo& workbookInfo() const { return m_workbookInfo; }
    const ExcelSheetPreview& currentSheet() const { return m_currentSheet; }
    int currentSheetIndex() const { return m_currentSheetIndex; }
    bool hasWorkbook() const { return !m_currentSheet.grid.isEmpty(); }

    OperationResult openWorkbook(const QString& filePath);
    OperationResult loadSheetAtIndex(int index);
    void clearWorkbook();

    OperationResult exportToFile(const QString& filePath, const QVariantMap& columnConfig);
    OperationResult exportToFolder(const QString& folderPath, const QVariantMap& columnConfig);
    OperationResult classifyToFolder(const QString& outputFolderPath,
                                       const QString& trajectoryFolderPath,
                                       const QVariantMap& columnConfig);
    OperationResult importTrajectoryFolder(const QString& folderPath);

    OperationResult beginScreenshotTasks(const QVariantMap& columnConfig);
    QVariantMap nextScreenshotTask();
    void cancelScreenshotTasks();

    QUrl suggestedExportFileUrl() const;
    QUrl suggestedExportFolderUrl() const;
    bool exportUsesFolder() const;

signals:
    void importProgress(int percentage);
    void workbookCleared();
    void sheetChanged();

private:
    BusinessColumnSelection makeColumnSelection(const QVariantMap& columnConfig) const;
    bool collectBusinessRows(const BusinessColumnSelection& selection,
                             BusinessWorkbookRowsResult& result,
                             QString& errorMessage) const;
    OperationResult makeClassifyResult(const BusinessClassifyResult& result);
    bool advanceScreenshotSheet(QString& errorMessage);

    TrajectoryImportManager* m_importManager = nullptr;
    ExcelParserManager* m_excelParser = nullptr;

    ExcelWorkbookInfo m_workbookInfo;
    ExcelSheetPreview m_currentSheet;
    QString m_fileName;
    QString m_workbookStatusMessage;
    int m_currentSheetIndex = -1;

    bool m_screenshotIterActive = false;
    BusinessColumnSelection m_screenshotSelection;
    QList<int> m_screenshotSheetIndices;
    int m_screenshotNextSheetListIndex = 0;
    QList<BusinessExportRow> m_screenshotPendingRows;
    int m_screenshotNextRowIndex = 0;
};

#endif // BUSINESSDATAMANAGER_H
