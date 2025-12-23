#include "ExcelDataReader.h"
#include "ErrorHandler.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCoreApplication>
#include <limits>
#include <algorithm>

// QXlsx includes
#include "xlsxdocument.h"

QXLSX_USE_NAMESPACE

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
        if (range.rowCount() <= 1) {
            QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), "Excel文件中没有数据行（只有表头或完全为空）");
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Check for reasonable data size to prevent memory issues
        int totalCells = range.rowCount() * range.columnCount();
        if (totalCells > 1000000) { // More than 1M cells
            qWarning() << "Large dataset detected:" << totalCells << "cells. This may take some time to process.";
        }
        
        // Parse header row to determine column indices
        QMap<QString, int> columnMap;
        if (!parseHeaderRow(worksheet, columnMap)) {
            QString errorMsg = HANDLE_DATA_ERROR(fileInfo.fileName(), "无法解析Excel文件的表头。请确保文件包含必要的列：车牌号、经度、纬度、上报时间");
            emit errorOccurred(errorMsg);
            return false;
        }
        
        // Parse data rows with comprehensive error handling
        int totalRows = range.rowCount();
        int processedRows = 0;
        int validRecords = 0;
        int skippedRows = 0;
        QStringList errorSummary;
        
        emit loadingProgress(0);
        
        for (int row = 2; row <= totalRows; ++row) {  // Start from row 2 (skip header)
            VehicleRecord record;
            QString rowError;
            
            if (parseDataRow(xlsx, row, columnMap, record, rowError)) {
                if (record.isValid()) {
                    // Additional validation for coordinate ranges
                    if (!record.isInChinaRange()) {
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
                int progress = (processedRows * 100) / (totalRows - 1);
                emit loadingProgress(progress);
                
                // Check for memory pressure during processing
                if (processedRows % 1000 == 0) {
                    QCoreApplication::processEvents(); // Allow UI updates and prevent freezing
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

QStringList ExcelDataReader::getAvailableDataFiles() const
{
    QDir dir(getDefaultDataPath());
    if (!dir.exists()) {
        return QStringList();
    }
    
    QStringList filters;
    filters << "*.xlsx" << "*.xls";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    
    QStringList filePaths;
    for (const QFileInfo& fileInfo : files) {
        filePaths.append(fileInfo.absoluteFilePath());
    }
    
    return filePaths;
}

ExcelDataReader::DataStatistics ExcelDataReader::getDataStatistics() const
{
    DataStatistics stats;
    stats.totalRecords = m_vehicleData.size();
    
    if (m_vehicleData.isEmpty()) {
        stats.uniqueVehicles = 0;
        stats.minSpeed = 0.0;
        stats.maxSpeed = 0.0;
        return stats;
    }
    
    // 计算统计信息
    QSet<QString> uniquePlates;
    QSet<QString> colors;
    stats.minSpeed = std::numeric_limits<double>::max();
    stats.maxSpeed = 0.0;
    stats.earliestTime = m_vehicleData.first().timestamp;
    stats.latestTime = m_vehicleData.first().timestamp;
    
    for (const auto& record : m_vehicleData) {
        uniquePlates.insert(record.plateNumber);
        if (!record.vehicleColor.isEmpty()) {
            colors.insert(record.vehicleColor);
        }
        
        stats.minSpeed = qMin(stats.minSpeed, record.speed);
        stats.maxSpeed = qMax(stats.maxSpeed, record.speed);
        
        if (record.timestamp < stats.earliestTime) {
            stats.earliestTime = record.timestamp;
        }
        if (record.timestamp > stats.latestTime) {
            stats.latestTime = record.timestamp;
        }
    }
    
    stats.uniqueVehicles = uniquePlates.size();
    stats.vehicleColors = colors.values();
    
    return stats;
}

bool ExcelDataReader::parseHeaderRow(Worksheet* pSheet, QMap<QString, int>& columnMap)
{
    // Expected column names (Chinese) - 预期的列名（中文）
    QStringList expectedColumns = {
        "车牌号", "车牌颜色", "速度(km/h)", "经度", "纬度", "方向", "海拔", "上报时间", "总里程"
    };
    // Get the dimension to know how many columns to check
    CellRange range = pSheet->dimension();
    int maxCol = 9;

    // Read header row (row 1)
    for (int col = 1; col <= maxCol; ++col) {
        QVariant cellValue = pSheet->read(1, col);
        if (cellValue.isNull() || cellValue.toString().trimmed().isEmpty()) {
            continue;
        }
        
        QString headerText = cellValue.toString().trimmed();
        
        // Try to match with expected columns
        for (const QString& expectedCol : expectedColumns) {
                if (headerText.contains(expectedCol, Qt::CaseInsensitive) ||
                    expectedCol.contains(headerText, Qt::CaseInsensitive)) {
                    columnMap[expectedCol] = col;
                    break;
                }
        }
    }
    
    // Check if we found the essential columns - 检查是否找到必需的列
    QStringList requiredColumns = {"车牌号", "经度", "纬度", "上报时间"};
    QStringList missingColumns;
    for (const QString& required : requiredColumns) {
        if (!columnMap.contains(required)) {
            missingColumns.append(required);
        }
    }
    
    if (!missingColumns.isEmpty()) {
        return false;
    }
    
    return true;
}

bool ExcelDataReader::parseDataRow(Document& xlsx, int row, const QMap<QString, int>& columnMap, VehicleRecord& record, QString& errorMessage)
{
    errorMessage.clear();
    
    try {
        // Parse plate number (required)
        if (columnMap.contains("车牌号")) {
            QVariant plateValue = xlsx.read(row, columnMap["车牌号"]);
            record.plateNumber = plateValue.toString().trimmed();
            if (record.plateNumber.isEmpty()) {
                errorMessage = "车牌号为空";
                return false;
            }
            
            // Validate plate number format (basic validation)
            if (record.plateNumber.length() < 6 || record.plateNumber.length() > 10) {
                errorMessage = QString("车牌号格式可能不正确: %1").arg(record.plateNumber);
                // Continue processing - this is just a warning
            }
        } else {
            errorMessage = "缺少车牌号列";
            return false;
        }
        
        // Parse vehicle color (optional)
        if (columnMap.contains("车牌颜色")) {
            QVariant colorValue = xlsx.read(row, columnMap["车牌颜色"]);
            record.vehicleColor = colorValue.toString().trimmed().contains("黄色") ? "yellow":"blue";
        }
        
        // Parse speed (optional, default to 0)
        record.speed = 0.0;
        if (columnMap.contains("速度(km/h)")) {
            QVariant speedValue = xlsx.read(row, columnMap["速度(km/h)"]);
            if (!speedValue.isNull() && !speedValue.toString().trimmed().isEmpty()) {
                bool ok;
                double speed = speedValue.toDouble(&ok);
                if (ok && speed >= 0 && speed <= 500.0) { // Reasonable speed range
                    record.speed = speed;
                } else if (!ok) {
                    errorMessage = QString("速度数据格式错误: %1").arg(speedValue.toString());
                } else {
                    errorMessage = QString("速度数据超出合理范围: %1").arg(speed);
                }
            }
        }
        
        // Parse longitude (required)
        if (columnMap.contains("经度")) {
            QVariant lngValue = xlsx.read(row, columnMap["经度"]);
            if (lngValue.isNull() || lngValue.toString().trimmed().isEmpty()) {
                errorMessage = "经度数据为空";
                return false;
            }
            
            bool ok;
            record.longitude = lngValue.toDouble(&ok);
            if (!ok) {
                errorMessage = QString("经度数据格式错误: %1").arg(lngValue.toString());
                return false;
            }
            
            // Validate longitude range
            if (record.longitude < -180.0 || record.longitude > 180.0) {
                errorMessage = QString("经度超出有效范围(-180到180): %1").arg(record.longitude);
                return false;
            }
        } else {
            errorMessage = "缺少经度列";
            return false;
        }
        
        // Parse latitude (required)
        if (columnMap.contains("纬度")) {
            QVariant latValue = xlsx.read(row, columnMap["纬度"]);
            if (latValue.isNull() || latValue.toString().trimmed().isEmpty()) {
                errorMessage = "纬度数据为空";
                return false;
            }
            
            bool ok;
            record.latitude = latValue.toDouble(&ok);
            if (!ok) {
                errorMessage = QString("纬度数据格式错误: %1").arg(latValue.toString());
                return false;
            }
            
            // Validate latitude range
            if (record.latitude < -90.0 || record.latitude > 90.0) {
                errorMessage = QString("纬度超出有效范围(-90到90): %1").arg(record.latitude);
                return false;
            }
        } else {
            errorMessage = "缺少纬度列";
            return false;
        }
        
        // Parse direction (optional, default to 0)
        record.direction = 0;
        if (columnMap.contains("方向")) {
            QVariant dirValue = xlsx.read(row, columnMap["方向"]);
            if (!dirValue.isNull() && !dirValue.toString().trimmed().isEmpty()) {
                bool ok;
                int direction = dirValue.toInt(&ok);
                if (ok && direction >= 0 && direction <= 360) {
                    record.direction = direction;
                } else if (!ok) {
                    errorMessage = QString("方向数据格式错误: %1").arg(dirValue.toString());
                } else {
                    errorMessage = QString("方向超出有效范围(0-360): %1").arg(direction);
                }
            }
        }
        
        // Parse distance (optional, default to 0)
        record.distance = 0.0;
        if (columnMap.contains("海拔")) {
            QVariant distValue = xlsx.read(row, columnMap["海拔"]);
            if (!distValue.isNull() && !distValue.toString().trimmed().isEmpty()) {
                bool ok;
                double distance = distValue.toDouble(&ok);
                if (ok && distance >= 0) {
                    record.distance = distance;
                } else if (!ok) {
                    errorMessage = QString("距离数据格式错误: %1").arg(distValue.toString());
                } else {
                    errorMessage = QString("距离数据为负值: %1").arg(distance);
                }
            }
        }
        
        // Parse timestamp (required)
        if (columnMap.contains("上报时间")) {
            QVariant timeValue = xlsx.read(row, columnMap["上报时间"]);
            if (timeValue.isNull() || timeValue.toString().trimmed().isEmpty()) {
                errorMessage = "上报时间为空";
                return false;
            }
            
            record.timestamp = parseTimestamp(timeValue);
            if (!record.timestamp.isValid()) {
                errorMessage = QString("时间格式错误: %1").arg(timeValue.toString());
                return false;
            }
        } else {
            errorMessage = "缺少上报时间列";
            return false;
        }
        
        // Parse total mileage (optional)
        if (columnMap.contains("总里程")) {
            QVariant mileageValue = xlsx.read(row, columnMap["总里程"]);
            record.totalMileage = mileageValue.toString().trimmed();
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

QDateTime ExcelDataReader::parseTimestamp(const QVariant& value)
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
// Performance optimization methods

QString ExcelDataReader::internString(const QString& str)
{
    if (!m_memoryOptimizationEnabled) {
        return str;
    }
    
    // Use string interning to reduce memory usage for repeated strings
    auto it = m_stringInternPool.find(str);
    if (it != m_stringInternPool.end()) {
        return it.value();
    }
    
    // Limit pool size to prevent memory bloat
    if (m_stringInternPool.size() > 10000) {
        m_stringInternPool.clear();
    }
    
    m_stringInternPool[str] = str;
    return str;
}

void ExcelDataReader::optimizeMemoryUsage()
{
    if (!m_memoryOptimizationEnabled) {
        return;
    }
    
    // Shrink containers to fit actual data size
    m_vehicleData.squeeze();
    
    // Clear string intern pool if it gets too large
    if (m_stringInternPool.size() > 5000) {
        m_stringInternPool.clear();
    }
    
    // Force garbage collection hint
    QCoreApplication::processEvents();
}
