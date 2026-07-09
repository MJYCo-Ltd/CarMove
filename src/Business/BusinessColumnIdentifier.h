#ifndef BUSINESSCOLUMNIDENTIFIER_H
#define BUSINESSCOLUMNIDENTIFIER_H

#include "ExcelDriver/ExcelPreviewTypes.h"

class BusinessColumnIdentifier
{
public:
    static void identifyColumns(ExcelSheetPreview& sheet);
    static QString formatColumnStatus(const ExcelSheetPreview& sheet);
    static void appendColumnStatus(ExcelSheetPreview& sheet);
};

#endif // BUSINESSCOLUMNIDENTIFIER_H
