#include "ParseData/OoxmlSaxExcelLoader.h"

#include "ParseData/ExcelCellFormatter.h"
#include "ParseData/ExcelFilePath.h"
#include "ParseData/ExcelSheetGridUtils.h"
#include "ErrorHandler.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QXmlStreamReader>

#include "xlsxreadsax.h"
#include "xlsxzipreader_p.h"

QXLSX_USE_NAMESPACE

namespace {

struct SheetInfo {
    QString name;
    QString path;
};

bool attributeLocalNameEquals(const QXmlStreamAttribute& attribute, QLatin1String localName)
{
    if (attribute.name() == localName) {
        return true;
    }
    const QString qualifiedName = attribute.qualifiedName().toString();
    return qualifiedName.endsWith(QLatin1Char(':') + localName);
}

QString attributeValueByLocalName(const QXmlStreamAttributes& attributes, QLatin1String localName)
{
    for (const QXmlStreamAttribute& attribute : attributes) {
        if (attributeLocalNameEquals(attribute, localName)) {
            return attribute.value().toString();
        }
    }
    return QString();
}

QHash<QString, QString> parseWorkbookRelationships(const QByteArray& relsXml)
{
    QHash<QString, QString> relationshipTargets;
    QXmlStreamReader reader(relsXml);

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("Relationship")) {
            continue;
        }

        const QString id = attributeValueByLocalName(reader.attributes(), QLatin1String("Id"));
        const QString target = attributeValueByLocalName(reader.attributes(), QLatin1String("Target"));
        if (!id.isEmpty() && !target.isEmpty()) {
            relationshipTargets.insert(id, target);
        }
    }

    return relationshipTargets;
}

QList<SheetInfo> parseWorkbookSheets(const QByteArray& workbookXml,
                                     const QHash<QString, QString>& relationshipTargets)
{
    QList<SheetInfo> sheets;
    QXmlStreamReader reader(workbookXml);

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("sheet")) {
            continue;
        }

        const QString sheetName = attributeValueByLocalName(reader.attributes(), QLatin1String("name"));
        const QString relationshipId = attributeValueByLocalName(reader.attributes(), QLatin1String("id"));
        if (sheetName.isEmpty() || relationshipId.isEmpty()) {
            continue;
        }

        const QString target = relationshipTargets.value(relationshipId);
        if (target.isEmpty()) {
            continue;
        }

        QString sheetPath = target;
        while (sheetPath.startsWith(QLatin1Char('/'))) {
            sheetPath.remove(0, 1);
        }
        if (!sheetPath.startsWith(QLatin1String("xl/"))) {
            sheetPath = QStringLiteral("xl/") + sheetPath;
        }

        sheets.append(SheetInfo{sheetName, sheetPath});
    }

    return sheets;
}

bool openZipReader(const QString& filePath, QFile& file, ZipReader*& zipReader)
{
    if (!ExcelFilePath::openReadableFile(file, filePath)) {
        return false;
    }

    zipReader = new ZipReader(&file);
    return zipReader->exists();
}

QList<SheetInfo> loadSheetInfos(const QString& filePath, QString& errorMessage)
{
    QFile file;
    ZipReader* zipReader = nullptr;
    if (!openZipReader(filePath, file, zipReader)) {
        errorMessage = HANDLE_FILE_ERROR(filePath, QStringLiteral("打开Excel文件"));
        return {};
    }

    const QByteArray workbookXml = zipReader->fileData(QStringLiteral("xl/workbook.xml"));
    const QByteArray relsXml = zipReader->fileData(QStringLiteral("xl/_rels/workbook.xml.rels"));
    delete zipReader;

    if (workbookXml.isEmpty() || relsXml.isEmpty()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("不是有效的 Excel 工作簿"));
        return {};
    }

    const QHash<QString, QString> relationshipTargets = parseWorkbookRelationships(relsXml);
    return parseWorkbookSheets(workbookXml, relationshipTargets);
}

struct SaxPreviewContext {
    ExcelPreviewLimits limits;
    QMap<int, QMap<int, QString>> cells;
    bool truncated = false;
};

bool saxPreviewCellCallback(const QXlsx::sax_cell& cell, SaxPreviewContext* context)
{
    if (cell.row <= 0 || cell.col <= 0) {
        return true;
    }

    if (cell.row > context->limits.maxRows) {
        context->truncated = true;
        return false;
    }
    if (cell.col > context->limits.maxColumns) {
        context->truncated = true;
        return true;
    }

    const QString text = ExcelCellFormatter::formatVariant(cell.value).trimmed();
    if (text.isEmpty()) {
        return true;
    }

    context->cells[cell.row][cell.col] = text;
    return true;
}

} // namespace

namespace OoxmlSaxExcelLoader {

bool inspectWorkbook(const QString& filePath, QStringList& sheetNames, QString& errorMessage)
{
    errorMessage.clear();
    sheetNames.clear();

    const QList<SheetInfo> sheets = loadSheetInfos(filePath, errorMessage);
    if (sheets.isEmpty()) {
        if (errorMessage.isEmpty()) {
            errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                             QStringLiteral("Excel文件中没有找到工作表"));
        }
        return false;
    }

    sheetNames.reserve(sheets.size());
    for (const SheetInfo& sheet : sheets) {
        sheetNames.append(sheet.name);
    }
    return true;
}

bool loadSheetPreview(const QString& filePath,
                      int sheetIndex,
                      const ExcelPreviewLimits& limits,
                      ExcelSheetPreview& sheet,
                      QString& errorMessage)
{
    errorMessage.clear();
    sheet = ExcelSheetPreview{};

    const QList<SheetInfo> sheets = loadSheetInfos(filePath, errorMessage);
    if (sheets.isEmpty()) {
        return false;
    }

    if (sheetIndex < 0 || sheetIndex >= sheets.size()) {
        errorMessage = QStringLiteral("工作表索引无效");
        return false;
    }

    const SheetInfo sheetInfo = sheets.at(sheetIndex);

    QFile file;
    ZipReader* zipReader = nullptr;
    if (!openZipReader(filePath, file, zipReader)) {
        errorMessage = HANDLE_FILE_ERROR(filePath, QStringLiteral("打开Excel文件"));
        return false;
    }

    QXlsx::sax_options options;
    QStringList sharedStrings = QXlsx::load_shared_strings_all(*zipReader);
    const QByteArray sheetXml = zipReader->fileData(sheetInfo.path);

    SaxPreviewContext context;
    context.limits = limits;

    const bool parsed = QXlsx::read_sheet_xml_sax(
        sheetXml,
        options,
        &sharedStrings,
        [&context](const QXlsx::sax_cell& cell) {
            return saxPreviewCellCallback(cell, &context);
        });

    delete zipReader;

    if (!parsed || context.cells.isEmpty()) {
        errorMessage = HANDLE_DATA_ERROR(QFileInfo(filePath).fileName(),
                                         QStringLiteral("无法读取工作表 %1").arg(sheetInfo.name));
        return false;
    }

    sheet.name = sheetInfo.name;
    ExcelSheetGridUtils::buildGridFromSparseCells(context.cells, sheet);
    ExcelSheetGridUtils::compressGridToUsedColumns(sheet.grid, sheet.columnCount);

    if (sheet.grid.isEmpty()) {
        sheet.statusMessage = QStringLiteral("工作表为空");
        return true;
    }

    sheet.statusMessage =
        ExcelSheetGridUtils::formatSheetStatus(sheet.grid.size(), sheet.columnCount, context.truncated, limits);
    return true;
}

} // namespace OoxmlSaxExcelLoader
