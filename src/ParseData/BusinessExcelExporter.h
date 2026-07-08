#pragma once

#include "ParseData/ExcelPreviewLoader.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

struct BusinessExportOptions {
    int startDateOrdinal = 0;
    int endDateOrdinal = 0;
    bool singleDateColumn = false;
    bool singleColumnIsStart = true;
    int dayOffset = 0;
};

struct BusinessExportRow {
    QString plate;
    QDate startDate;
    QDate endDate;
};

struct ResolvedSheetExportColumns {
    int plateDataColumn = -1;
    int startDataColumn = -1;
    int endDataColumn = -1;
};

struct BusinessClassifyResult {
    int movedFiles = 0;
    int missingFiles = 0;
    int exportedCsvFiles = 0;
    int exportedRows = 0;
    QStringList missingEntries;
    QStringList skippedSheetNames;
};

class BusinessExcelExporter
{
public:
    static QString fileNameForSheet(const QString& excelFileName,
                                    const QString& sheetName,
                                    bool appendSheetName);

    static QString folderNameForSheet(const QString& excelFileName,
                                      const QString& sheetName,
                                      bool appendSheetName);

    static ResolvedSheetExportColumns resolveColumns(const ExcelSheetPreview& sheet,
                                                     const BusinessExportOptions& options);

    static QList<BusinessExportRow> collectRows(const ExcelSheetPreview& sheet,
                                                const ResolvedSheetExportColumns& columns,
                                                const BusinessExportOptions& options);

    static void sortRowsByPlate(QList<BusinessExportRow>& rows);

    static void deduplicateRows(QList<BusinessExportRow>& rows);

    static QString formatSheetError(const QString& sheetName, const QString& message);

    static bool exportSheetToCsv(const ExcelSheetPreview& sheet,
                                 const BusinessExportOptions& options,
                                 const QString& filePath,
                                 QString& errorMessage,
                                 int* exportedRows = nullptr);

    static bool exportWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                          const BusinessExportOptions& options,
                                          const QString& outputDirectory,
                                          QString& errorMessage,
                                          int* exportedRows = nullptr,
                                          int* exportedFiles = nullptr,
                                          QStringList* exportedFilePaths = nullptr,
                                          QStringList* skippedSheetNames = nullptr);

    static bool classifySheet(const ExcelSheetPreview& sheet,
                              const BusinessExportOptions& options,
                              const QString& trajectoryDirectory,
                              const QString& outputDirectory,
                              const QString& excelFileName,
                              bool appendSheetName,
                              QString& errorMessage,
                              BusinessClassifyResult* result = nullptr);

    static bool classifyWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                            const BusinessExportOptions& options,
                                            const QString& trajectoryDirectory,
                                            const QString& outputDirectory,
                                            QString& errorMessage,
                                            BusinessClassifyResult* result = nullptr);
};
