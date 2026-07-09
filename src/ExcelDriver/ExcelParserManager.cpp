#include "ExcelDriver/ExcelParserManager.h"

#include "ExcelDriver/ExcelParserPreviewInternal.h"
#include "ExcelDriver/ExcelBackend.h"
#include "ExcelDriver/ExcelFilePath.h"
#include "ExcelDriver/QXlsxExcelLoader.h"
#include "ExcelDriver/XlsxioExcelLoader.h"
#include "Core/ErrorHandler.h"
#include "Core/AppLogger.h"
#include "Core/LocalFilePath.h"

#include <QFileInfo>
#include <algorithm>

ExcelParserManager::ExcelParserManager(QObject* parent)
    : QObject(parent)
{
}

bool ExcelParserManager::inspectWorkbook(const QString& filePath,
                                         ExcelWorkbookInfo& info,
                                         QString& errorMessage)
{
    return ExcelParserPreviewInternal::inspectWorkbook(filePath, info, errorMessage);
}

bool ExcelParserManager::loadSheet(const ExcelWorkbookInfo& info,
                                   int sheetIndex,
                                   ExcelSheetPreview& sheet,
                                   QString& errorMessage)
{
    return ExcelParserPreviewInternal::loadSheet(info, sheetIndex, sheet, errorMessage);
}

bool ExcelParserManager::loadVehicleRecords(const QString& filePath,
                                            QList<VehicleRecord>& records,
                                            QString& errorMessage,
                                            const ProgressCallback& onProgress)
{
    records.clear();
    errorMessage.clear();

    const QString localPath = LocalFilePath::normalizeLocalFilePath(filePath);
    QFileInfo fileInfo;
    if (!ExcelFilePath::validateExcelFile(localPath, fileInfo, errorMessage)) {
        return false;
    }

    if (fileInfo.size() == 0) {
        errorMessage = HANDLE_DATA_ERROR(fileInfo.fileName(), QStringLiteral("文件为空"));
        return false;
    }

    const qint64 fileSize = fileInfo.size();
    const QString suffix = fileInfo.suffix().toLower();
    const ExcelBackend::ReaderType readerType = ExcelBackend::selectReader(fileSize, suffix);

    AppLogger::info(QStringLiteral("读取 %1 (%2 bytes)，自动选择 Excel 解析后端")
                        .arg(fileInfo.fileName())
                        .arg(fileSize));

    const auto progressCallback = [this, onProgress](int percentage) {
        if (onProgress) {
            onProgress(percentage);
        }
        emit loadingProgress(percentage);
    };

    QString loadError;
    bool loaded = false;

    try {
        if (readerType == ExcelBackend::ReaderType::Xlsxio) {
            loaded = XlsxioExcelLoader::load(localPath, records, progressCallback, loadError);
        } else {
            loaded = QXlsxExcelLoader::load(localPath, records, progressCallback, loadError);
            if (!loaded && suffix == QLatin1String("xlsx")) {
                AppLogger::warn(QStringLiteral("主解析器加载失败，已自动切换备用解析器：%1").arg(loadError));
                loadError.clear();
                loaded = XlsxioExcelLoader::load(localPath, records, progressCallback, loadError);
            }
        }
    } catch (const std::bad_alloc&) {
        errorMessage = HANDLE_MEMORY_ERROR(QStringLiteral("加载Excel文件"));
        return false;
    } catch (const std::exception& e) {
        errorMessage = HANDLE_SYSTEM_ERROR(QStringLiteral("读取Excel文件"), e.what());
        return false;
    } catch (...) {
        errorMessage = HANDLE_SYSTEM_ERROR(QStringLiteral("读取Excel文件"), QStringLiteral("未知异常"));
        return false;
    }

    if (!loaded) {
        errorMessage = loadError;
        return false;
    }

    try {
        std::sort(records.begin(),
                  records.end(),
                  [](const VehicleRecord& a, const VehicleRecord& b) {
                      return a.timestamp < b.timestamp;
                  });
    } catch (const std::exception& e) {
        AppLogger::warn(QStringLiteral("按时间排序失败: %1").arg(e.what()));
    }

    AppLogger::info(QStringLiteral("成功加载 %1 条有效记录").arg(records.size()));
    emit loadingProgress(100);
    return !records.isEmpty();
}

QString ExcelParserManager::formatWorkbookStatus(const ExcelWorkbookInfo& info)
{
    switch (info.previewMode) {
    case ExcelPreviewMode::Streaming:
        return QStringLiteral("共 %1 个工作表（大文件按需预览，每个工作表最多 %2 行 × %3 列）")
            .arg(info.sheetNames.size())
            .arg(info.limits.maxRows)
            .arg(info.limits.maxColumns);
    case ExcelPreviewMode::AlternateXml:
        return QStringLiteral("共 %1 个工作表（兼容格式）").arg(info.sheetNames.size());
    case ExcelPreviewMode::Standard:
    default:
        return QStringLiteral("共 %1 个工作表").arg(info.sheetNames.size());
    }
}
