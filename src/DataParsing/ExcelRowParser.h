#ifndef EXCELROWPARSER_H
#define EXCELROWPARSER_H

#include "Domain/TrajectoryTypes.h"
#include <QHash>
#include <QVariant>

class ExcelRowParser
{
public:
    static bool parseRow(const QHash<int, QVariant>& cells,
                         TrajectoryPoint& record,
                         QString& errorMessage);

    static QVariant parseAndValidateField(const QVariant& cellValue,
                                          const QString& dataType,
                                          const QString& fieldName,
                                          QString& errorMessage);
};

#endif // EXCELROWPARSER_H
