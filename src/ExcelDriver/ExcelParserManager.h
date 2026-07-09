#ifndef EXCELPARSERMANAGER_H
#define EXCELPARSERMANAGER_H

#include "ExcelDriver/ExcelPreviewTypes.h"
#include "Domain/TrajectoryTypes.h"

#include <QObject>
#include <functional>

/**
 * @brief Excel 解析管理器：统一预览与轨迹读取入口，内部按文件大小与格式自动选择解析后端。
 *
 * 外部模块不依赖 QXlsx、xlsxio 或 OOXML SAX 等具体实现。
 */
class ExcelParserManager : public QObject
{
    Q_OBJECT

public:
    using VehicleRecord = TrajectoryPoint;
    using ProgressCallback = std::function<void(int percentage)>;

    explicit ExcelParserManager(QObject* parent = nullptr);

    bool inspectWorkbook(const QString& filePath,
                         ExcelWorkbookInfo& info,
                         QString& errorMessage);
    bool loadSheet(const ExcelWorkbookInfo& info,
                   int sheetIndex,
                   ExcelSheetPreview& sheet,
                   QString& errorMessage);
    bool loadVehicleRecords(const QString& filePath,
                            QList<VehicleRecord>& records,
                            QString& errorMessage,
                            const ProgressCallback& onProgress = {});

    static QString formatWorkbookStatus(const ExcelWorkbookInfo& info);

signals:
    void loadingProgress(int percentage);
};

#endif // EXCELPARSERMANAGER_H
