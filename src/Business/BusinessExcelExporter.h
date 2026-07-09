#pragma once

#include "Business/BusinessWorkbookTypes.h"
#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QHash>
#include <QStringList>

class ExcelParserManager;

class BusinessExcelExporter
{
public:
    static QString fileNameForSheet(const QString& excelFileName,
                                    const QString& sheetName,
                                    bool appendSheetName);

    static QString folderNameForSheet(const QString& excelFileName,
                                      const QString& sheetName,
                                      bool appendSheetName);

    static bool exportRowsToCsv(const QList<BusinessExportRow>& rows,
                                const QString& filePath,
                                const QString& sheetName,
                                QString& errorMessage,
                                int* exportedRows = nullptr);

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

    static bool classifyRowsIntoFolder(const QList<BusinessExportRow>& rows,
                                         const QHash<QString, QString>& trajectoryIndex,
                                         const QString& targetFolderPath,
                                         QString& errorMessage,
                                         int* movedFiles = nullptr,
                                         int* missingFiles = nullptr,
                                         QStringList* missingEntries = nullptr);

    static bool moveTrajectoryFile(const QString& sourcePath,
                                   const QString& destinationPath,
                                   QString& errorMessage);

    static bool classifyWorkbookToDirectory(const ExcelWorkbookInfo& workbookInfo,
                                            const ExcelSheetPreview& referenceSheet,
                                            int currentSheetIndex,
                                            const BusinessColumnSelection& selection,
                                            ExcelParserManager& parser,
                                            const QString& trajectoryDirectory,
                                            const QString& outputDirectory,
                                            QString& errorMessage,
                                            BusinessClassifyResult* result = nullptr);
};
