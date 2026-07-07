#ifndef EXCELBACKEND_H
#define EXCELBACKEND_H

#include <QtGlobal>
#include <QString>

namespace ExcelBackend {

constexpr qint64 LargeFileThresholdBytes = 100LL * 1024 * 1024;

enum class ReaderType {
    QXlsx,
    Xlsxio,
    OoxmlSax
};

inline ReaderType selectReader(qint64 fileSizeBytes, const QString& suffix)
{
    // xlsxio 仅支持 .xlsx，.xls 无论大小均走 QXlsx
    if (suffix == QLatin1String("xls")) {
        return ReaderType::QXlsx;
    }
    if (fileSizeBytes >= LargeFileThresholdBytes) {
        return ReaderType::Xlsxio;
    }
    return ReaderType::QXlsx;
}

inline QString readerTypeName(ReaderType type)
{
    switch (type) {
    case ReaderType::QXlsx:
        return QStringLiteral("QXlsx");
    case ReaderType::Xlsxio:
        return QStringLiteral("xlsxio");
    case ReaderType::OoxmlSax:
        return QStringLiteral("OoxmlSax");
    }
    return QStringLiteral("unknown");
}

} // namespace ExcelBackend

#endif // EXCELBACKEND_H
