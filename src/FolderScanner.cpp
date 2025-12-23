#include "FolderScanner.h"
#include "ExcelDataReader.h"
#include "ErrorHandler.h"
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QCoreApplication>
#include <QRegularExpression>
#include <algorithm>
#include <numeric>

FolderScanner::FolderScanner(QObject *parent)
    : QObject(parent)
{
}

void FolderScanner::scanFolder(const QString& folderPath)
{
    m_vehicleList.clear();
    
    // Comprehensive folder validation
    if (folderPath.isEmpty()) {
        emit scanError(HANDLE_FILE_ERROR("", "文件夹路径为空"));
        return;
    }
    
    QDir dir(folderPath);
    if (!dir.exists()) {
        emit scanError(HANDLE_FILE_ERROR(folderPath, "访问文件夹"));
        return;
    }
    
    // Check if we have read permissions
    QFileInfo dirInfo(folderPath);
    if (!dirInfo.isReadable()) {
        emit scanError(HANDLE_FILE_ERROR(folderPath, "读取文件夹"));
        return;
    }
    
    // Get all Excel files with comprehensive filtering
    QStringList filters;
    filters << "*.xlsx" << "*.xls" << "*.XLSX" << "*.XLS"; // Include uppercase extensions
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable);
    
    if (files.isEmpty()) {
        // Check if there are any files at all
        QFileInfoList allFiles = dir.entryInfoList(QDir::Files);
        if (allFiles.isEmpty()) {
            emit scanError(QString("文件夹为空：%1\n\n请选择包含Excel文件的文件夹。").arg(folderPath));
        } else {
            emit scanError(QString("文件夹中没有找到Excel文件：%1\n\n"
                                  "找到 %2 个其他文件，但没有.xlsx或.xls格式的文件。\n"
                                  "请确保文件夹包含车辆轨迹数据的Excel文件。")
                          .arg(folderPath).arg(allFiles.size()));
        }
        return;
    }
    
    // Check for very large number of files
    if (files.size() > 1000) {
        qWarning() << "Large number of Excel files detected:" << files.size() << "This may take some time.";
    }
    
    // Map to store vehicle information aggregated by plate number from filenames
    QMap<QString, VehicleInfo> vehicleMap;
    
    int processedFiles = 0;
    int validFiles = 0;
    int invalidFiles = 0;
    QStringList errorSummary;
    
    emit scanProgress(0);
    
    try {
        // 正则表达式匹配文件名中的车牌号
        // 支持格式：冀JY8706-2025-05-23.xlsx 或 冀JY8706.xlsx
        QRegularExpression plateRegex(u8"([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})");
        
        // Process each Excel file to extract vehicle plate number from filename
        for (const QFileInfo& fileInfo : files) {
            QString fileName = fileInfo.fileName();
            QString filePath = fileInfo.absoluteFilePath();
            
            // Additional file validation
            if (fileInfo.size() == 0) {
                qWarning() << "Skipping empty file:" << filePath;
                invalidFiles++;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QString("文件为空: %1").arg(fileName));
                }
                continue;
            }
            
            // Check if file is too large (>500MB)
            if (fileInfo.size() > 500 * 1024 * 1024) {
                qWarning() << "Skipping very large file:" << filePath << "Size:" << fileInfo.size();
                invalidFiles++;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QString("文件过大: %1").arg(fileName));
                }
                continue;
            }
            
            // Extract plate number from filename
            QRegularExpressionMatch match = plateRegex.match(fileName);
            
            if (match.hasMatch()) {
                QString plateNumber = match.captured(1);
                
                // Update or create vehicle info
                if (vehicleMap.contains(plateNumber)) {
                    // Add this file to existing vehicle's file list
                    VehicleInfo& info = vehicleMap[plateNumber];
                    if (!info.filePaths.contains(filePath)) {
                        info.filePaths.append(filePath);
                        info.recordCount++; // 简单计数文件数量，实际记录数在加载时才知道
                    }
                } else {
                    // Create new vehicle info
                    VehicleInfo info;
                    info.plateNumber = plateNumber;
                    info.filePaths.append(filePath);
                    info.recordCount = 1; // 文件数量
                    // 时间戳将在实际加载数据时设置
                    info.firstTimestamp = QDateTime();
                    info.lastTimestamp = QDateTime();
                    
                    vehicleMap[plateNumber] = info;
                }
                
                validFiles++;
            } else {
                // 文件名不符合车牌号格式
                qWarning() << "无法从文件名提取车牌号:" << fileName;
                invalidFiles++;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QString("文件名格式不正确: %1").arg(fileName));
                }
            }
            
            processedFiles++;
            int progress = (processedFiles * 100) / files.size();
            emit scanProgress(progress);
            
            // Allow UI updates during long operations
            if (processedFiles % 50 == 0) {
                QCoreApplication::processEvents();
            }
        }
        
        // Validate final results
        if (vehicleMap.isEmpty()) {
            QString errorMsg = QString("扫描完成，但没有找到有效的车辆数据\n\n"
                                     "处理了 %1 个文件，其中 %2 个有效，%3 个无效。\n\n"
                                     "可能的原因：\n"
                                     "• 文件名格式不符合要求（应为：车牌号-日期.xlsx）\n"
                                     "• 文件名中没有包含车牌号\n\n"
                                     "建议：检查文件名是否以车牌号开头，例如：冀JY8706-2025-05-23.xlsx")
                              .arg(processedFiles).arg(validFiles).arg(invalidFiles);
            
            if (!errorSummary.isEmpty()) {
                errorMsg += QString("\n\n错误示例：\n%1").arg(errorSummary.join("\n"));
            }
            
            emit scanError(errorMsg);
            return;
        }
        
        // Convert map to list and sort by plate number
        m_vehicleList.clear();
        
        for (auto it = vehicleMap.begin(); it != vehicleMap.end(); ++it) {
            m_vehicleList.append(it.value());
        }
        
        // Sort vehicle list by plate number
        std::sort(m_vehicleList.begin(), m_vehicleList.end(), 
                  [](const VehicleInfo& a, const VehicleInfo& b) {
                      return a.plateNumber < b.plateNumber;
                  });
        
        // Log comprehensive statistics
        int totalFiles = std::accumulate(m_vehicleList.begin(), m_vehicleList.end(), 0,
                                        [](int sum, const VehicleInfo& info) { return sum + info.filePaths.size(); });
        
        QString successMsg = QString("扫描完成，找到 %1 个车辆，共 %2 个文件")
                            .arg(m_vehicleList.size()).arg(totalFiles);
        
        if (invalidFiles > 0) {
            successMsg += QString("\n处理了 %1 个文件，其中 %2 个有效，%3 个无效")
                         .arg(processedFiles).arg(validFiles).arg(invalidFiles);
        }
        
        // 显示每个车辆的文件数量
        for (const auto& info : m_vehicleList) {
        }
        
        // Show warning if many files are invalid
        if (invalidFiles > validFiles * 0.2) { // More than 20% invalid
            qWarning() << QString("警告：较多文件无效 (%1/%2)，请检查文件命名格式")
                         .arg(invalidFiles).arg(processedFiles);
        }
        
        emit scanCompleted(m_vehicleList);
        
    } catch (const std::bad_alloc& e) {
        emit scanError(HANDLE_MEMORY_ERROR("扫描文件夹"));
    } catch (const std::exception& e) {
        emit scanError(HANDLE_SYSTEM_ERROR("扫描文件夹", e.what()));
    } catch (...) {
        emit scanError(HANDLE_SYSTEM_ERROR("扫描文件夹", "未知异常"));
    }
}

QList<FolderScanner::VehicleInfo> FolderScanner::getVehicleList() const
{
    return m_vehicleList;
}
