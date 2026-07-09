#include "DataParsing/ExcelDataReader.h"
#include "ExcelDriver/ExcelBackend.h"
#include "ExcelDriver/ExcelFilePath.h"
#include "ExcelDriver/QXlsxExcelLoader.h"
#include "ExcelDriver/XlsxioExcelLoader.h"
#include "Core/ErrorHandler.h"
#include "Core/AppLogger.h"

#include <QFileInfo>
#include <algorithm>

ExcelDataReader::ExcelDataReader(QObject* parent)
    : QObject(parent)
{
}

bool ExcelDataReader::loadExcelFile(const QString& filePath)
{
    m_vehicleData.clear();

    const QString localPath = ExcelFilePath::normalizeLocalFilePath(filePath);
    QFileInfo fileInfo;
    QString validationError;
    if (!ExcelFilePath::validateExcelFile(localPath, fileInfo, validationError)) {
        emit errorOccurred(validationError);
        return false;
    }

    if (fileInfo.size() == 0) {
        emit errorOccurred(HANDLE_DATA_ERROR(fileInfo.fileName(), QStringLiteral("文件为空")));
        return false;
    }

    const qint64 fileSize = fileInfo.size();
    const QString suffix = fileInfo.suffix().toLower();

    const ExcelBackend::ReaderType readerType =
        ExcelBackend::selectReader(fileSize, suffix);

    AppLogger::info(QStringLiteral("读取 %1 (%2 bytes)，使用 %3 后端")
                        .arg(fileInfo.fileName())
                        .arg(fileSize)
                        .arg(ExcelBackend::readerTypeName(readerType)));

    const auto progressCallback = [this](int percentage) {
        emit loadingProgress(percentage);
    };

    QString loadError;
    bool loaded = false;

    try {
        if (readerType == ExcelBackend::ReaderType::Xlsxio) {
            loaded = XlsxioExcelLoader::load(localPath, m_vehicleData, progressCallback, loadError);
        } else {
            loaded = QXlsxExcelLoader::load(localPath, m_vehicleData, progressCallback, loadError);
            if (!loaded && suffix == QLatin1String("xlsx")) {
                AppLogger::warn(QStringLiteral("QXlsx 加载失败，回退到 xlsxio：%1").arg(loadError));
                loadError.clear();
                loaded =
                    XlsxioExcelLoader::load(localPath, m_vehicleData, progressCallback, loadError);
            }
        }
    } catch (const std::bad_alloc&) {
        emit errorOccurred(HANDLE_MEMORY_ERROR(QStringLiteral("加载Excel文件")));
        return false;
    } catch (const std::exception& e) {
        emit errorOccurred(HANDLE_SYSTEM_ERROR(QStringLiteral("读取Excel文件"), e.what()));
        return false;
    } catch (...) {
        emit errorOccurred(HANDLE_SYSTEM_ERROR(QStringLiteral("读取Excel文件"), QStringLiteral("未知异常")));
        return false;
    }

    if (!loaded) {
        emit errorOccurred(loadError);
        return false;
    }

    return finalizeLoadedData(fileInfo.fileName());
}

bool ExcelDataReader::finalizeLoadedData(const QString& fileName)
{
    try {
        std::sort(m_vehicleData.begin(),
                  m_vehicleData.end(),
                  [](const VehicleRecord& a, const VehicleRecord& b) {
                      return a.timestamp < b.timestamp;
                  });
    } catch (const std::exception& e) {
        AppLogger::warn(QStringLiteral("按时间排序失败: %1").arg(e.what()));
    }

    AppLogger::info(QStringLiteral("成功加载 %1 条有效记录").arg(m_vehicleData.size()));

    emit dataLoaded(m_vehicleData);
    emit loadingProgress(100);
    return !m_vehicleData.isEmpty();
}

QList<ExcelDataReader::VehicleRecord> ExcelDataReader::getVehicleData() const
{
    return m_vehicleData;
}
