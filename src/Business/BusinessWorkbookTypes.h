#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>

struct BusinessColumnSelection {
    int startColumnNumber = -1;
    int endColumnNumber = -1;
    QString singleTimeRole;
    int dayOffset = 0;

    static BusinessColumnSelection fromUi(int startColumnNumber,
                                        int endColumnNumber,
                                        const QString& singleTimeRole,
                                        int dayOffset);
};
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

struct BusinessSheetRows {
    QString sheetName;
    QList<BusinessExportRow> rows;
};

struct BusinessWorkbookRowsResult {
    QList<BusinessSheetRows> sheets;
    QStringList skippedSheetNames;
    QStringList anomalyMessages;

    bool isEmpty() const { return sheets.isEmpty(); }

    int totalRowCount() const;

    QList<BusinessExportRow> allRowsFlat() const;
};
