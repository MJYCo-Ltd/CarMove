#ifndef EXCELPREVIEWMODE_H
#define EXCELPREVIEWMODE_H

#include <QAbstractTableModel>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "ParseData/ExcelPreviewLoader.h"

#include <QUrl>
#include <QVariant>

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

public:
    explicit ExcelPreviewModel(QObject* parent = nullptr);

    QString filePath() const { return m_workbookInfo.filePath; }
    QString fileName() const { return m_fileName; }
    QString statusMessage() const { return m_statusMessage; }
    QString currentSheetStatus() const { return m_currentSheet.statusMessage; }
    QString errorMessage() const { return m_errorMessage; }
    bool hasData() const;
    bool loading() const { return m_loading; }
    QStringList sheetNames() const { return m_workbookInfo.sheetNames; }
    int sheetCount() const { return m_workbookInfo.sheetNames.size(); }
    int currentSheetIndex() const { return m_currentSheetIndex; }
    int previewRowCount() const { return m_currentSheet.grid.size(); }
    int previewDataColumnCount() const { return m_currentSheet.columnCount; }
    int previewColumnCount() const
    {
        return m_currentSheet.columnCount > 0 ? m_currentSheet.columnCount + 1 : 0;
    }
    int detectedDateColumnCount() const;
    int detectedPlateColumnCount() const;
    int defaultPlateColumnNumber() const;
    QVariantList dateColumnOptions() const;
    QVariantList plateColumnOptions() const;

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
    Q_INVOKABLE bool exportBusinessCsv(const QString& filePath,
                                       int startColumnNumber,
                                       int endColumnNumber,
                                       const QString& singleTimeRole,
                                       int dayOffset);
    Q_INVOKABLE bool exportBusinessCsvToFolder(const QString& folderPath,
                                               int startColumnNumber,
                                               int endColumnNumber,
                                               const QString& singleTimeRole,
                                               int dayOffset);

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

private:
    bool loadSheetAtIndex(int index);
    void releaseCurrentSheet();
    void setLoading(bool loading);
    void setErrorMessage(const QString& message);
    void setStatusMessage(const QString& message);

    ExcelWorkbookInfo m_workbookInfo;
    ExcelSheetPreview m_currentSheet;
    QString m_fileName;
    QString m_statusMessage;
    QString m_errorMessage;
    bool m_loading = false;
    int m_currentSheetIndex = -1;
};

#endif // EXCELPREVIEWMODE_H
