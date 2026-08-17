#include "DataParsing/LicensePlate.h"

#include <QRegularExpression>

namespace LicensePlate {

namespace {

QString stripPlateDecorations(const QString& text)
{
    QString plate = text.trimmed();
    static const QRegularExpression colorSuffix(
        QStringLiteral(R"([（(]\s*黄色\s*[）)])"));
    plate.remove(colorSuffix);
    plate.remove(QRegularExpression(QStringLiteral("\\s+")));
    return plate.trimmed();
}

} // namespace

QString capturePattern()
{
    return QStringLiteral(
        "([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼使领]"
        "(?:[A-HJ-NP-Z][A-HJ-NP-Z0-9]{5}[DF]"
        "|[A-HJ-NP-Z][A-HJ-NP-Z0-9]{4}[A-HJ-NP-Z0-9挂学警港澳]?))");
}

QString canonicalPlateNumber(const QString& text)
{
    return stripPlateDecorations(text);
}

bool isChineseVehiclePlate(const QString& text)
{
    const QString normalized = stripPlateDecorations(text).toUpper();
    if (normalized.size() < 7 || normalized.size() > 8) {
        return false;
    }

    static const QRegularExpression regex(
        QStringLiteral("^") + capturePattern() + QLatin1Char('$'));
    return regex.match(normalized).hasMatch();
}

} // namespace LicensePlate
