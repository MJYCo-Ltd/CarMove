#include "ExcelDriver/ExcelTrajectoryManager.h"

#include "ExcelDriver/ExcelParserManager.h"
#include "Core/ErrorHandler.h"
#include "Core/AppLogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>
#include <numeric>

ExcelTrajectoryManager::ExcelTrajectoryManager(QObject* parent)
    : QObject(parent)
    , m_parser(new ExcelParserManager(this))
{
}

void ExcelTrajectoryManager::scanFolder(const QString& folderPath)
{
    m_vehicleList.clear();

    if (folderPath.isEmpty()) {
        emit scanError(HANDLE_FILE_ERROR(QString(), QStringLiteral("文件夹路径为空")));
        return;
    }

    QDir dir(folderPath);
    if (!dir.exists()) {
        emit scanError(HANDLE_FILE_ERROR(folderPath, QStringLiteral("访问文件夹")));
        return;
    }

    const QFileInfo dirInfo(folderPath);
    if (!dirInfo.isReadable()) {
        emit scanError(HANDLE_FILE_ERROR(folderPath, QStringLiteral("读取文件夹")));
        return;
    }

    QStringList filters;
    filters << QStringLiteral("*.xlsx") << QStringLiteral("*.xls")
            << QStringLiteral("*.XLSX") << QStringLiteral("*.XLS");
    const QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable);

    if (files.isEmpty()) {
        const QFileInfoList allFiles = dir.entryInfoList(QDir::Files);
        if (allFiles.isEmpty()) {
            emit scanError(QStringLiteral("文件夹为空：%1\n\n请选择包含Excel文件的文件夹。").arg(folderPath));
        } else {
            emit scanError(QStringLiteral("文件夹中没有找到Excel文件：%1\n\n"
                                          "找到 %2 个其他文件，但没有.xlsx或.xls格式的文件。")
                               .arg(folderPath)
                               .arg(allFiles.size()));
        }
        return;
    }

    if (files.size() > 1000) {
        AppLogger::warn(QStringLiteral("检测到大量 Excel 文件: %1，可能需要较长时间").arg(files.size()));
    }

    QMap<QString, VehicleSummary> vehicleMap;
    int processedFiles = 0;
    int validFiles = 0;
    int invalidFiles = 0;
    QStringList errorSummary;

    emit scanProgress(0);

    try {
        const QRegularExpression plateRegex(
            u8"([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})");

        for (const QFileInfo& fileInfo : files) {
            const QString fileName = fileInfo.fileName();
            const QString filePath = fileInfo.absoluteFilePath();

            if (fileInfo.size() == 0) {
                ++invalidFiles;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QStringLiteral("文件为空: %1").arg(fileName));
                }
                continue;
            }

            if (fileInfo.size() > 500LL * 1024 * 1024) {
                ++invalidFiles;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QStringLiteral("文件过大: %1").arg(fileName));
                }
                continue;
            }

            const QRegularExpressionMatch match = plateRegex.match(fileName);
            if (match.hasMatch()) {
                const QString plateNumber = match.captured(1);
                if (vehicleMap.contains(plateNumber)) {
                    VehicleSummary& info = vehicleMap[plateNumber];
                    if (!info.sourceFilePaths.contains(filePath)) {
                        info.sourceFilePaths.append(filePath);
                        ++info.totalPointCount;
                    }
                } else {
                    VehicleSummary info;
                    info.plateNumber = plateNumber;
                    info.sourceFilePaths.append(filePath);
                    info.totalPointCount = 1;
                    vehicleMap.insert(plateNumber, info);
                }
                ++validFiles;
            } else {
                ++invalidFiles;
                if (errorSummary.size() < 5) {
                    errorSummary.append(QStringLiteral("文件名格式不正确: %1").arg(fileName));
                }
            }

            ++processedFiles;
            emit scanProgress((processedFiles * 100) / files.size());

            if (processedFiles % 50 == 0) {
                QCoreApplication::processEvents();
            }
        }

        if (vehicleMap.isEmpty()) {
            QString errorMsg =
                QStringLiteral("扫描完成，但没有找到有效的车辆数据\n\n"
                               "处理了 %1 个文件，其中 %2 个有效，%3 个无效。")
                    .arg(processedFiles)
                    .arg(validFiles)
                    .arg(invalidFiles);
            if (!errorSummary.isEmpty()) {
                errorMsg += QStringLiteral("\n\n错误示例：\n%1").arg(errorSummary.join(QStringLiteral("\n")));
            }
            emit scanError(errorMsg);
            return;
        }

        m_vehicleList = vehicleMap.values();
        std::sort(m_vehicleList.begin(), m_vehicleList.end(),
                  [](const VehicleSummary& a, const VehicleSummary& b) {
                      return a.plateNumber < b.plateNumber;
                  });

        if (invalidFiles > validFiles * 0.2) {
            AppLogger::warn(QStringLiteral("警告：较多文件无效 (%1/%2)，请检查文件命名格式")
                                .arg(invalidFiles)
                                .arg(processedFiles));
        }

        emit scanCompleted(m_vehicleList);
    } catch (const std::bad_alloc&) {
        emit scanError(HANDLE_MEMORY_ERROR(QStringLiteral("扫描文件夹")));
    } catch (const std::exception& e) {
        emit scanError(HANDLE_SYSTEM_ERROR(QStringLiteral("扫描文件夹"), e.what()));
    } catch (...) {
        emit scanError(HANDLE_SYSTEM_ERROR(QStringLiteral("扫描文件夹"), QStringLiteral("未知异常")));
    }
}

ExcelTrajectoryManager::TrajectoryLoadResult ExcelTrajectoryManager::loadTrajectory(
    const VehicleSummary& vehicle,
    const QString& plateNumber,
    const QDate& startDate,
    const QDate& endDate,
    bool hasDateRange)
{
    TrajectoryLoadResult result;

    if (vehicle.sourceFilePaths.isEmpty()) {
        result.errorMessage = QStringLiteral("无法找到车辆文件: %1").arg(plateNumber);
        return result;
    }

    QList<TrajectoryPoint> allRecords;
    const int totalFiles = vehicle.sourceFilePaths.size();
    int processedFiles = 0;

    for (const QString& filePath : vehicle.sourceFilePaths) {
        QList<TrajectoryPoint> fileRecords;
        QString loadError;

        const int currentFileIndex = processedFiles;
        const bool loadSuccess = m_parser->loadVehicleRecords(
            filePath,
            fileRecords,
            loadError,
            [this, currentFileIndex, totalFiles](int fileProgress) {
                emit loadProgress(((currentFileIndex * 100) + fileProgress) / totalFiles);
            });

        if (loadSuccess) {
            for (const TrajectoryPoint& record : fileRecords) {
                if (record.plateNumber == plateNumber) {
                    allRecords.append(record);
                }
            }
        } else {
            AppLogger::warn(QStringLiteral("读取文件失败: %1 | %2").arg(filePath, loadError));
        }

        ++processedFiles;
        emit loadProgress((processedFiles * 100) / totalFiles);
    }

    if (hasDateRange && startDate.isValid() && endDate.isValid()) {
        QList<TrajectoryPoint> filteredRecords;
        filteredRecords.reserve(allRecords.size());
        for (const TrajectoryPoint& record : allRecords) {
            const QDate recordDate = record.timestamp.date();
            if (recordDate >= startDate && recordDate <= endDate) {
                filteredRecords.append(record);
            }
        }
        allRecords = filteredRecords;
    }

    if (allRecords.isEmpty()) {
        result.errorMessage = QStringLiteral("未找到车辆轨迹记录: %1").arg(plateNumber);
        return result;
    }

    result.points = allRecords;
    result.success = true;
    return result;
}
