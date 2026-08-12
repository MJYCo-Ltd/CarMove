#pragma once

#include "Business/BusinessWorkbookTypes.h"
#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QStringList>

class ExcelParserManager;

class BusinessExcelExporter
{
public:
    static QString fileNameForSheet(const QString& excelFileName,
                                    const QString& sheetName,
                                    bool appendSheetName);

    static bool exportRowsToXlsx(const QList<BusinessExportRow>& rows,
                                 const QString& filePath,
                                 const QString& sheetName,
                                 QString& errorMessage,
                                 int* exportedRows = nullptr);

    static bool writeAnomalyReport(const QStringList& anomalyMessages,
                                   const QString& sourceExcelFilePath,
                                   const QString& outputDirectory,
                                   QString& errorMessage,
                                   QString* reportFilePath = nullptr);

    static bool exportWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                          const ExcelSheetPreview& referenceSheet,
                                          int currentSheetIndex,
                                          const BusinessColumnSelection& selection,
                                          ExcelParserManager& parser,
                                          const QString& outputDirectory,
                                          QString& errorMessage,
                                          int* exportedRows = nullptr,
                                          int* exportedFiles = nullptr,
                                          QStringList* exportedFilePaths = nullptr,
                                          QStringList* skippedSheetNames = nullptr);

};
