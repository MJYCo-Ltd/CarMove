#pragma once

#include "ParseData/BusinessWorkbookTypes.h"

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>

class BusinessCsvWriter
{
public:
    static QString formatExportDate(const QDate& date);

    static bool writeRowsToCsv(const QList<BusinessExportRow>& rows,
                               const QString& filePath,
                               QString& errorMessage);
};
