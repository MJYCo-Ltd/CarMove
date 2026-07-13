#ifndef EXCELPREVIEWMODE_H
#define EXCELPREVIEWMODE_H

#include <QAbstractTableModel>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include <QUrl>
#include <QVariantMap>

#include <QFutureWatcher>
#include <QtConcurrent>

#include "Business/BusinessDataManager.h"

class ExcelPreviewModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY filePathChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString currentSheetStatus READ currentSheetStatus NOTIFY currentSheetChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QStringList sheetNames READ sheetNames NOTIFY sheetsChanged)
    Q_PROPERTY(int sheetCount READ sheetCount NOTIFY sheetsChanged)
    Q_PROPERTY(int currentSheetIndex READ currentSheetIndex WRITE setCurrentSheetIndex NOTIFY currentSheetIndexChanged)
    Q_PROPERTY(int previewRowCount READ previewRowCount NOTIFY currentSheetChanged)
    Q_PROPERTY(int previewColumnCount READ previewColumnCount NOTIFY currentSheetChanged)
    Q_PROPERTY(int previewDataColumnCount READ previewDataColumnCount NOTIFY currentSheetChanged)
    Q_PROPERTY(int detectedDateColumnCount READ detectedDateColumnCount NOTIFY currentSheetChanged)
    Q_PROPERTY(int detectedPlateColumnCount READ detectedPlateColumnCount NOTIFY currentSheetChanged)
    Q_PROPERTY(int defaultPlateColumnNumber READ defaultPlateColumnNumber NOTIFY currentSheetChanged)
    Q_PROPERTY(QVariantList dateColumnOptions READ dateColumnOptions NOTIFY currentSheetChanged)
    Q_PROPERTY(QVariantList plateColumnOptions READ plateColumnOptions NOTIFY currentSheetChanged)
    Q_PROPERTY(BusinessDataManager* businessData READ businessData CONSTANT)

public:
    explicit ExcelPreviewModel(QObject* parent = nullptr);

    QString filePath() const;
    QString fileName() const;
    QString statusMessage() const { return m_statusMessage; }
    QString currentSheetStatus() const;
    QString errorMessage() const { return m_errorMessage; }
    bool hasData() const;
    bool loading() const { return m_loading; }
    QStringList sheetNames() const;
    int sheetCount() const;
    int currentSheetIndex() const;
    int previewRowCount() const;
    int previewColumnCount() const;
    int previewDataColumnCount() const;
    int detectedDateColumnCount() const;
    int detectedPlateColumnCount() const;
    int defaultPlateColumnNumber() const;
    QVariantList dateColumnOptions() const;
    QVariantList plateColumnOptions() const;
    BusinessDataManager* businessData() const { return m_dataManager; }

    static constexpr int RowNumberColumn = 0;

    Q_INVOKABLE bool loadFile(const QString& filePath);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setCurrentSheetIndex(int index);
    Q_INVOKABLE bool isPlateColumn(int tableColumnIndex) const;
    Q_INVOKABLE bool isDateColumn(int tableColumnIndex) const;
    Q_INVOKABLE QString suggestedExportFileName() const;
    Q_INVOKABLE QUrl suggestedExportFileUrl() const;
    Q_INVOKABLE QUrl suggestedExportFolderUrl() const;
    Q_INVOKABLE bool exportUsesFolder() const;
    Q_INVOKABLE bool exportBusinessWithConfig(const QString& filePath, const QVariantMap& columnConfig);
    Q_INVOKABLE bool exportBusinessFolderWithConfig(const QString& folderPath,
                                                    const QVariantMap& columnConfig);
    Q_INVOKABLE bool classifyWithConfig(const QString& outputFolderPath,
                                        const QString& trajectoryFolderPath,
                                        const QVariantMap& columnConfig);
    Q_INVOKABLE bool beginScreenshotTasks(const QVariantMap& columnConfig);
    Q_INVOKABLE QVariantMap nextScreenshotTask();
    Q_INVOKABLE void cancelScreenshotTasks();
    Q_INVOKABLE bool importTrajectoryFolderToDatabase(const QString& folderPath);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void filePathChanged();
    void statusMessageChanged();
    void errorMessageChanged();
    void hasDataChanged();
    void loadingChanged();
    void sheetsChanged();
    void currentSheetIndexChanged();
    void currentSheetChanged();
    void loadFinished(bool success);

private slots:
    void onBusinessSheetChanged();
    void onBusinessWorkbookCleared();
    void onImportProgress(int percentage);

private:
    void applyOperationResult(const BusinessDataManager::OperationResult& result,
                              bool updateStatus = true);
    void setLoading(bool loading);
    void setErrorMessage(const QString& message);
    void setStatusMessage(const QString& message);
    void resetModelFromBusinessData();
    void finishOpenWorkbookLoad(int generation, const BusinessDataManager::OpenWorkbookResult& result);
    void finishSheetLoad(int generation, const BusinessDataManager::SheetLoadResult& result);

    BusinessDataManager* m_dataManager = nullptr;
    QString m_statusMessage;
    QString m_errorMessage;
    bool m_loading = false;
    int m_loadGeneration = 0;
    QFutureWatcher<BusinessDataManager::OpenWorkbookResult>* m_openWorkbookWatcher = nullptr;
    QFutureWatcher<BusinessDataManager::SheetLoadResult>* m_sheetLoadWatcher = nullptr;
};

#endif // EXCELPREVIEWMODE_H
