#include "DataParsing/ExcelRowParser.h"

#include "Core/ConfigManager.h"
#include "DataParsing/DateTimeParser.h"
#include "DataParsing/LicensePlate.h"

bool ExcelRowParser::parseRow(const QHash<int, QVariant>& cells,
                              TrajectoryPoint& record,
                              QString& errorMessage)
{
    errorMessage.clear();

    try {
        for (const auto& mapping : ConfigManager::GetInstance()->getExcelFieldMappings()) {
            if (!mapping.isMapped()) {
                continue;
            }

            const QVariant cellValue = cells.value(mapping.columnIndex);
            QString fieldError;

            if (mapping.fieldName == QStringLiteral("车牌号")) {
                record.plateNumber = LicensePlate::canonicalPlateNumber(cellValue.toString());
                if (record.plateNumber.isEmpty() && mapping.isRequired) {
                    errorMessage = QStringLiteral("车牌号为空");
                    return false;
                }
            } else if (mapping.fieldName == QStringLiteral("车牌颜色")) {
                record.vehicleColor = cellValue.toString().trimmed().contains(QStringLiteral("黄色"))
                                          ? QStringLiteral("yellow")
                                          : QStringLiteral("blue");
            } else if (mapping.fieldName == QStringLiteral("速度")) {
                const QVariant validatedValue =
                    parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    record.speed = 0.0;
                } else {
                    record.speed = validatedValue.toDouble();
                    if (record.speed < 0 || record.speed > 500.0) {
                        if (mapping.isRequired) {
                            errorMessage = QStringLiteral("速度数据超出合理范围: %1").arg(record.speed);
                            return false;
                        }
                        record.speed = 0.0;
                    }
                }
            } else if (mapping.fieldName == QStringLiteral("经度")) {
                const QVariant validatedValue =
                    parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    errorMessage = fieldError;
                    return false;
                }
                record.longitude = validatedValue.toDouble();
                if (record.longitude < -180.0 || record.longitude > 180.0) {
                    errorMessage = QStringLiteral("经度超出有效范围(-180到180): %1").arg(record.longitude);
                    return false;
                }
            } else if (mapping.fieldName == QStringLiteral("纬度")) {
                const QVariant validatedValue =
                    parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    errorMessage = fieldError;
                    return false;
                }
                record.latitude = validatedValue.toDouble();
                if (record.latitude < -90.0 || record.latitude > 90.0) {
                    errorMessage = QStringLiteral("纬度超出有效范围(-90到90): %1").arg(record.latitude);
                    return false;
                }
            } else if (mapping.fieldName == QStringLiteral("方向")) {
                const QVariant validatedValue =
                    parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    record.direction = 0;
                } else {
                    record.direction = validatedValue.toInt();
                    if (record.direction < 0 || record.direction > 360) {
                        if (mapping.isRequired) {
                            errorMessage = QStringLiteral("方向超出有效范围(0-360): %1").arg(record.direction);
                            return false;
                        }
                        record.direction = 0;
                    }
                }
            } else if (mapping.fieldName == QStringLiteral("海拔") || mapping.fieldName == QStringLiteral("距离")) {
                const QVariant validatedValue =
                    parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    record.distance = 0.0;
                } else {
                    record.distance = validatedValue.toDouble();
                    if (record.distance < 0) {
                        if (mapping.isRequired) {
                            errorMessage = QStringLiteral("距离数据为负值: %1").arg(record.distance);
                            return false;
                        }
                        record.distance = 0.0;
                    }
                }
            } else if (mapping.fieldName == QStringLiteral("上报时间")) {
                record.timestamp = DateTimeParser::parseDateTime(cellValue);
                if (!record.timestamp.isValid()) {
                    errorMessage = QStringLiteral("时间格式错误: %1").arg(cellValue.toString());
                    return false;
                }
            } else if (mapping.fieldName == QStringLiteral("总里程")) {
                record.totalMileage = cellValue.toString().trimmed();
            }
        }

        return true;
    } catch (const std::exception& e) {
        errorMessage = QStringLiteral("解析数据行异常: %1").arg(e.what());
        return false;
    } catch (...) {
        errorMessage = QStringLiteral("解析数据行时发生未知异常");
        return false;
    }
}

QVariant ExcelRowParser::parseAndValidateField(const QVariant& cellValue,
                                               const QString& dataType,
                                               const QString& fieldName,
                                               QString& errorMessage)
{
    errorMessage.clear();

    if (cellValue.isNull() || cellValue.toString().trimmed().isEmpty()) {
        errorMessage = QStringLiteral("%1数据为空").arg(fieldName);
        return QVariant();
    }

    if (dataType == QStringLiteral("number")) {
        bool ok = false;
        const double value = cellValue.toDouble(&ok);
        if (!ok) {
            errorMessage = QStringLiteral("%1数据格式错误: %2").arg(fieldName, cellValue.toString());
            return QVariant();
        }
        return value;
    }

    if (dataType == QStringLiteral("datetime")) {
        const QDateTime dateTime = DateTimeParser::parseDateTime(cellValue);
        if (!dateTime.isValid()) {
            errorMessage = QStringLiteral("%1时间格式错误: %2").arg(fieldName, cellValue.toString());
            return QVariant();
        }
        return dateTime;
    }

    if (dataType == QStringLiteral("text")) {
        return cellValue.toString().trimmed();
    }

    return cellValue.toString().trimmed();
}
