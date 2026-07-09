#ifndef EXCELPREVIEWLOADER_H
#define EXCELPREVIEWLOADER_H

#include "ExcelDriver/ExcelBackend.h"

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

struct ExcelWorkbookInfo {
    QString filePath;
    QStringList sheetNames;
    ExcelBackend::ReaderType readerType = ExcelBackend::ReaderType::QXlsx;
    ExcelPreviewLimits limits;
    qint64 fileSizeBytes = 0;
};

class ExcelPreviewLoader
{
public:
    static bool inspectWorkbook(const QString& filePath,
                                ExcelWorkbookInfo& info,
                                QString& errorMessage);

    static bool loadSheet(const ExcelWorkbookInfo& info,
                          int sheetIndex,
                          ExcelSheetPreview& sheet,
                          QString& errorMessage);
};

#endif // EXCELPREVIEWLOADER_H
