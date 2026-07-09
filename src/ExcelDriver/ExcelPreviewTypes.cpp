#include "ExcelDriver/ExcelPreviewTypes.h"

#include "ExcelDriver/ExcelBackend.h"

ExcelPreviewLimits ExcelPreviewLimits::forFileSize(qint64 fileSizeBytes)
{
    ExcelPreviewLimits limits;
    if (fileSizeBytes >= ExcelBackend::LargeFileThresholdBytes) {
        limits.maxRows = 300;
        limits.maxColumns = 40;
    } else if (fileSizeBytes >= 50LL * 1024 * 1024) {
        limits.maxRows = 1000;
        limits.maxColumns = 60;
    }
    return limits;
}

ExcelPreviewLimits ExcelPreviewLimits::forExport()
{
    ExcelPreviewLimits limits;
    limits.maxRows = 2'000'000;
    limits.maxColumns = 512;
    return limits;
}
