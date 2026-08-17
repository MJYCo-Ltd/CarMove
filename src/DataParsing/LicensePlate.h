#pragma once

#include <QString>

namespace LicensePlate {

/// 用于文件名等场景的捕获正则（含捕获组，不含锚点）。
QString capturePattern();

/// 去掉空白和末尾「(黄色)」类标注。
QString canonicalPlateNumber(const QString& text);

bool isChineseVehiclePlate(const QString& text);

} // namespace LicensePlate
