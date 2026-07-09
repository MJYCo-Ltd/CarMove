#ifndef EXCELPREVIEWTYPES_H
#define EXCELPREVIEWTYPES_H

#include <QString>
#include <QStringList>
#include <QVector>

struct ExcelSheetPreview {
    QString name;
    QVector<QVector<QString>> grid;
    QVector<int> originalRowNumbers;
    QVector<bool> isPlateColumn;
    QVector<bool> isDateColumn;
    int columnCount = 0;
    QString statusMessage;
};

struct ExcelPreviewLimits {
    int maxRows = 5000;
    int maxColumns = 100;

    static ExcelPreviewLimits forFileSize(qint64 fileSizeBytes);
    static ExcelPreviewLimits forExport();
};

enum class ExcelPreviewMode {
    Standard,
    Streaming,
    AlternateXml
};

struct ExcelWorkbookInfo {
    QString filePath;
    QStringList sheetNames;
    ExcelPreviewMode previewMode = ExcelPreviewMode::Standard;
    ExcelPreviewLimits limits;
    qint64 fileSizeBytes = 0;
};

#endif // EXCELPREVIEWTYPES_H
