#include "ExcelDataReader.h"
#include "ErrorHandler.h"
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QCoreApplication>
#include <algorithm>

// QXlsx includes
#include "xlsxdocument.h"

QXLSX_USE_NAMESPACE
#include "ConfigManager.h"
ExcelDataReader::ExcelDataReader(QObject *parent)
    : QObject(parent)
{
}

bool ExcelDataReader::loadExcelFile(const QString& filePath)
{
    // Clear previous data
    m_vehicleData.clear();
    
    // Comprehensive file access validation
    QFileInfo fileInfo(filePath);
    
    // Check if file exists
    if (!fileInfo.exists()) {
        QString errorMsg = HANDLE_FILE_ERROR(filePath, "读取");
        emit errorOccurred(errorMsg);
        return false;
    }
    
    // Check if it's actually a file (not a directory)
    if (!fileInfo.isFile()) {
        QString errorMsg = HANDLE_FILE_ERROR(filePath, "读取");
        emit errorOccurred(errorMsg);
        return false;
    }
    
    // Check file permissions
    if (!fileInfo.isReadable()) {
        QString errorMsg = HANDLE_FILE_ERROR(filePath, "读取");
        emit errorOccurred(errorMsg);
        return false;
    }
    
    // Check file size (avoid loading extremely large files that might cause memory issues)
    qint64 fileSize = fileInfo.size();
    if (fileSize == 0) {
        QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), "文件为空");
        emit errorOccurred(errorMsg);
        return false;
    }
    
    // Warn about very large files (>100MB)
    if (fileSize > 100 * 1024 * 1024) {
        qWarning() << "Large file detected:" << filePath << "Size:" << fileSize << "bytes";
        // Continue processing but warn user
    }
    
    // Check file extension
    QString suffix = fileInfo.suffix().toLower();
    if (suffix != "xlsx" && suffix != "xls") {
        QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), 
                                           QString("不支持的文件格式: %1。支持的格式：.xlsx, .xls").arg(suffix));
        emit errorOccurred(errorMsg);
        return false;
    }
    
    try {
        // Open Excel document with error handling
        Document xlsx(filePath);
        if (!xlsx.load()) {
            QString errorMsg = HANDLE_FILE_ERROR(filePath, "打开Excel文件");
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Get the first worksheet
        Worksheet* worksheet = xlsx.currentWorksheet();
        if (!worksheet) {
            QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), "Excel文件中没有找到工作表");
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Get the dimension of the worksheet
        CellRange range = worksheet->dimension();
        if (range.rowCount() < ConfigManager::GetInstance()->getExcelDataStartRow()) {
            QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), 
                                               QString("Excel文件行数不足。数据起始行为%1，但文件只有%2行")
                                               .arg(ConfigManager::GetInstance()->getExcelDataStartRow())
                                               .arg(range.rowCount()));
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Check for reasonable data size to prevent memory issues
        int totalCells = range.rowCount() * range.columnCount();
        if (totalCells > 1000000) { // More than 1M cells
            qWarning() << "Large dataset detected:" << totalCells << "cells. This may take some time to process.";
        }
        
        // Parse data rows using column mapping with comprehensive error handling
        int totalRows = range.rowCount();
        int processedRows = 0;
        int validRecords = 0;
        int skippedRows = 0;
        QStringList errorSummary;
        
        emit loadingProgress(0);
        
        for (int row = ConfigManager::GetInstance()->getExcelDataStartRow(); row <= totalRows; ++row) {
            VehicleRecord record;
            QString rowError;
            
            if (parseDataRowWithMapping(xlsx, row, record, rowError)) {
                if (record.isValid()) {
                    // Additional validation for coordinate ranges
                    if (!record.isInChinaRange()) {
                        qWarning() << QString("警告：车辆 %1 在第 %2 行的坐标可能不在中国境内: (%3, %4)")
                                     .arg(record.plateNumber).arg(row).arg(record.latitude).arg(record.longitude);
                    }
                    
                    // Check for reasonable speed values (0-300 km/h)
                    if (record.speed > 300.0) {
                        qWarning() << QString("警告：车辆 %1 在第 %2 行的速度异常高: %3 km/h")
                                     .arg(record.plateNumber).arg(row).arg(record.speed);
                    }
                    
                    m_vehicleData.append(record);
                    validRecords++;
                } else {
                    skippedRows++;
                    QString validationError = QString("第%1行数据验证失败：车牌号=%2").arg(row).arg(record.plateNumber);
                    if (errorSummary.size() < 10) { // Limit error summary to prevent spam
                        errorSummary.append(validationError);
                    }
                }
            } else {
                skippedRows++;
                QString parseError = QString("第%1行数据解析失败").arg(row);
                if (!rowError.isEmpty()) {
                    parseError += QString("：%1").arg(rowError);
                }
                if (errorSummary.size() < 10) {
                    errorSummary.append(parseError);
                }
            }
            
            processedRows++;
            
            // Update progress every 100 rows or at the end
            if (processedRows % 100 == 0 || row == totalRows) {
                int progress = (processedRows * 100) / (totalRows - ConfigManager::GetInstance()->getExcelDataStartRow() + 1);
                emit loadingProgress(progress);
                
                // Allow UI updates and prevent freezing for large datasets
                if (processedRows % 1000 == 0) {
                    QCoreApplication::processEvents();
                }
            }
        }
        
        // Validate final results
        if (validRecords == 0) {
            QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), 
                                               QString("文件中没有有效的车辆数据。处理了%1行，跳过了%2行无效数据。")
                                               .arg(processedRows).arg(skippedRows));
            if (!errorSummary.isEmpty()) {
                errorMsg += QString("\n\n错误示例：\n%1").arg(errorSummary.join("\n"));
            }
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Sort data by timestamp (按时间顺序排序数据)
        try {
            std::sort(m_vehicleData.begin(), m_vehicleData.end(), 
                      [](const VehicleRecord& a, const VehicleRecord& b) {
                          return a.timestamp < b.timestamp;
                      });
        } catch (const std::exception& e) {
            qWarning() << "Error sorting data by timestamp:" << e.what();
            // Continue without sorting - data is still usable
        }
        
        // Log success with statistics
        QString successMsg = QString("成功加载 %1 条有效记录，共处理 %2 行数据")
                            .arg(validRecords).arg(processedRows);
        if (skippedRows > 0) {
            successMsg += QString("，跳过 %1 行无效数据").arg(skippedRows);
        }
        
        // Show warning if too many rows were skipped
        if (skippedRows > processedRows * 0.1) { // More than 10% skipped
            qWarning() << QString("警告：跳过了较多无效数据行 (%1/%2)，请检查数据质量")
                         .arg(skippedRows).arg(processedRows);
        }
        
        emit dataLoaded(m_vehicleData);
        emit loadingProgress(100);
        
        return validRecords > 0;
        
    } catch (const std::bad_alloc& e) {
        QString errorMsg = HANDLE_MEMORY_ERROR("加载Excel文件");
        emit errorOccurred(errorMsg);
        return false;
    } catch (const std::exception& e) {
        QString errorMsg = HANDLE_SYSTEM_ERROR("读取Excel文件", e.what());
        emit errorOccurred(errorMsg);
        return false;
    } catch (...) {
        QString errorMsg = HANDLE_SYSTEM_ERROR("读取Excel文件", "未知异常");
        emit errorOccurred(errorMsg);
        return false;
    }
}

QList<ExcelDataReader::VehicleRecord> ExcelDataReader::getVehicleData() const
{
    return m_vehicleData;
}

QStringList ExcelDataReader::getUniqueVehicles() const
{
    QStringList vehicles;
    for (const auto& record : m_vehicleData) {
        if (!vehicles.contains(record.plateNumber)) {
            vehicles.append(record.plateNumber);
        }
    }
    return vehicles;
}

QList<ExcelDataReader::VehicleRecord> ExcelDataReader::getVehicleRecords(const QString& plateNumber) const
{
    QList<VehicleRecord> vehicleRecords;
    for (const auto& record : m_vehicleData) {
        if (record.plateNumber == plateNumber) {
            vehicleRecords.append(record);
        }
    }
    return vehicleRecords;
}

bool ExcelDataReader::parseDataRowWithMapping(Document& xlsx, int row,
                                              VehicleRecord& record, QString& errorMessage)
{
    errorMessage.clear();
    
    try {
        // Parse each field based on the column mapping
        for (const auto& mapping : ConfigManager::GetInstance()->getExcelFieldMappings()) {
            if (!mapping.isMapped()) {
                continue; // Skip unmapped fields
            }
            
            QVariant cellValue = xlsx.read(row, mapping.columnIndex);
            QString fieldError;
            
            // Parse and validate the field based on its type and name
            if (mapping.fieldName == "车牌号") {
                record.plateNumber = cellValue.toString().trimmed();
                if (record.plateNumber.isEmpty() && mapping.isRequired) {
                    errorMessage = "车牌号为空";
                    return false;
                }
                
                // Validate plate number format (basic validation)
                if (!record.plateNumber.isEmpty() && 
                    (record.plateNumber.length() < 6 || record.plateNumber.length() > 10)) {
                    errorMessage = QString("车牌号格式可能不正确: %1").arg(record.plateNumber);
                    // Continue processing - this is just a warning
                }
            }
            else if (mapping.fieldName == "车牌颜色") {
                record.vehicleColor = cellValue.toString().trimmed().contains("黄色") ? "yellow" : "blue";
            }
            else if (mapping.fieldName == "速度") {
                QVariant validatedValue = parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    // For optional fields, use default value
                    record.speed = 0.0;
                } else {
                    record.speed = validatedValue.toDouble();
                    // Check reasonable speed range
                    if (record.speed < 0 || record.speed > 500.0) {
                        errorMessage = QString("速度数据超出合理范围: %1").arg(record.speed);
                        if (mapping.isRequired) return false;
                        record.speed = 0.0; // Use default for optional field
                    }
                }
            }
            else if (mapping.fieldName == "经度") {
                QVariant validatedValue = parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    errorMessage = fieldError;
                    return false;
                }
                record.longitude = validatedValue.toDouble();
                
                // Validate longitude range
                if (record.longitude < -180.0 || record.longitude > 180.0) {
                    errorMessage = QString("经度超出有效范围(-180到180): %1").arg(record.longitude);
                    return false;
                }
            }
            else if (mapping.fieldName == "纬度") {
                QVariant validatedValue = parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    errorMessage = fieldError;
                    return false;
                }
                record.latitude = validatedValue.toDouble();
                
                // Validate latitude range
                if (record.latitude < -90.0 || record.latitude > 90.0) {
                    errorMessage = QString("纬度超出有效范围(-90到90): %1").arg(record.latitude);
                    return false;
                }
            }
            else if (mapping.fieldName == "方向") {
                QVariant validatedValue = parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    record.direction = 0; // Default value
                } else {
                    record.direction = validatedValue.toInt();
                    if (record.direction < 0 || record.direction > 360) {
                        errorMessage = QString("方向超出有效范围(0-360): %1").arg(record.direction);
                        if (mapping.isRequired) return false;
                        record.direction = 0; // Use default for optional field
                    }
                }
            }
            else if (mapping.fieldName == "海拔" || mapping.fieldName == "距离") {
                QVariant validatedValue = parseAndValidateField(cellValue, mapping.dataType, mapping.fieldName, fieldError);
                if (!fieldError.isEmpty()) {
                    if (mapping.isRequired) {
                        errorMessage = fieldError;
                        return false;
                    }
                    record.distance = 0.0; // Default value
                } else {
                    record.distance = validatedValue.toDouble();
                    if (record.distance < 0) {
                        errorMessage = QString("距离数据为负值: %1").arg(record.distance);
                        if (mapping.isRequired) return false;
                        record.distance = 0.0; // Use default for optional field
                    }
                }
            }
            else if (mapping.fieldName == "上报时间") {
                record.timestamp = parseTimestamp(cellValue);
                if (!record.timestamp.isValid()) {
                    errorMessage = QString("时间格式错误: %1").arg(cellValue.toString());
                    return false;
                }
            }
            else if (mapping.fieldName == "总里程") {
                record.totalMileage = cellValue.toString().trimmed();
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        errorMessage = QString("解析数据行异常: %1").arg(e.what());
        return false;
    } catch (...) {
        errorMessage = "解析数据行时发生未知异常";
        return false;
    }
}

QVariant ExcelDataReader::parseAndValidateField(const QVariant& cellValue, const QString& dataType, 
                                               const QString& fieldName, QString& errorMessage) const
{
    errorMessage.clear();
    
    if (cellValue.isNull() || cellValue.toString().trimmed().isEmpty()) {
        errorMessage = QString("%1数据为空").arg(fieldName);
        return QVariant();
    }
    
    if (dataType == "number") {
        bool ok;
        double value = cellValue.toDouble(&ok);
        if (!ok) {
            errorMessage = QString("%1数据格式错误: %2").arg(fieldName).arg(cellValue.toString());
            return QVariant();
        }
        return value;
    }
    else if (dataType == "datetime") {
        QDateTime dateTime = parseTimestamp(cellValue);
        if (!dateTime.isValid()) {
            errorMessage = QString("%1时间格式错误: %2").arg(fieldName).arg(cellValue.toString());
            return QVariant();
        }
        return dateTime;
    }
    else if (dataType == "text") {
        return cellValue.toString().trimmed();
    }
    
    // Default: return as string
    return cellValue.toString().trimmed();
}

QDateTime ExcelDataReader::parseTimestamp(const QVariant& value) const
{
    if (value.isNull()) {
        return QDateTime();
    }
    
    // Try different parsing approaches
    
    // 1. If it's already a QDateTime
    if (value.type() == QVariant::DateTime) {
        return value.toDateTime();
    }
    
    // 2. If it's a string, try various formats
    if (value.type() == QVariant::String) {
        QString timeStr = value.toString().trimmed();
        if (timeStr.isEmpty()) {
            return QDateTime();
        }
        
        // Common timestamp formats - 常见的时间戳格式
        QStringList formats = {
            "yyyy-MM-dd hh:mm:ss",
            "yyyy/MM/dd hh:mm:ss", 
            "yyyy-MM-dd hh:mm",
            "yyyy/MM/dd hh:mm",
            "yyyy-MM-dd",
            "yyyy/MM/dd",
            "MM/dd/yyyy hh:mm:ss",
            "MM-dd-yyyy hh:mm:ss",
            "dd/MM/yyyy hh:mm:ss",
            "dd-MM-yyyy hh:mm:ss",
            "yyyy年MM月dd日 hh:mm:ss",  // 中文格式
            "yyyy年MM月dd日 hh时mm分ss秒",
            "yyyy年MM月dd日",
            "MM月dd日 hh:mm:ss",
            "hh:mm:ss"  // 仅时间格式
        };
        
        for (const QString& format : formats) {
            QDateTime dt = QDateTime::fromString(timeStr, format);
            if (dt.isValid()) {
                return dt;
            }
        }
        
        // Try ISO format
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (dt.isValid()) {
            return dt;
        }
    }
    
    // 3. If it's a number (Excel serial date)
    if (value.type() == QVariant::Double || value.type() == QVariant::Int) {
        double serialDate = value.toDouble();
        if (serialDate > 0) {
            // Excel serial date starts from 1900-01-01 (but Excel incorrectly treats 1900 as a leap year)
            // So we need to adjust: Excel day 1 = 1900-01-01, but QDate starts from different epoch
            QDate excelEpoch(1899, 12, 30);  // Excel's epoch adjusted for the leap year bug
            QDateTime dt = QDateTime(excelEpoch.addDays(static_cast<int>(serialDate)).startOfDay());
            
            // Add fractional part as time
            double fractionalPart = serialDate - static_cast<int>(serialDate);
            int totalSeconds = static_cast<int>(fractionalPart * 24 * 60 * 60);
            dt = dt.addSecs(totalSeconds);
            
            if (dt.isValid()) {
                return dt;
            }
        }
    }
    
    return QDateTime();
}

