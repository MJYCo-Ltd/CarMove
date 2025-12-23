#include "ErrorHandler.h"
#include <QFileInfo>
#include <QDir>

ErrorHandler::ErrorHandler(QObject *parent)
    : QObject(parent)
{
}

QString ErrorHandler::handleFileAccessError(const QString& filePath, const QString& operation)
{
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString userMessage;
    
    if (!fileInfo.exists()) {
        userMessage = QString("文件不存在：%1\n请检查文件路径是否正确。").arg(fileName);
    } else if (!fileInfo.isReadable()) {
        userMessage = QString("无法读取文件：%1\n请检查文件权限或文件是否被其他程序占用。").arg(fileName);
    } else if (!fileInfo.isFile()) {
        userMessage = QString("指定的路径不是文件：%1\n请选择正确的文件。").arg(fileName);
    } else if (operation == "write" && !fileInfo.isWritable()) {
        userMessage = QString("无法写入文件：%1\n请检查文件权限。").arg(fileName);
    } else {
        userMessage = QString("文件访问错误：%1\n操作：%2\n请稍后重试或联系技术支持。").arg(fileName).arg(operation);
    }
    
    qWarning() << "File access error:" << filePath << "Operation:" << operation;
    return userMessage;
}

QString ErrorHandler::handleDataFormatError(const QString& fileName, const QString& issue)
{
    QString userMessage;
    
    if (issue.contains("header", Qt::CaseInsensitive) || issue.contains("表头", Qt::CaseInsensitive)) {
        userMessage = QString("Excel文件格式错误：%1\n\n文件缺少必要的表头信息。\n"
                             "请确保Excel文件包含以下列：\n"
                             "• 车牌号\n• 经度\n• 纬度\n• 上报时间\n\n"
                             "建议：检查第一行是否包含正确的列标题。").arg(fileName);
    } else if (issue.contains("coordinate", Qt::CaseInsensitive) || issue.contains("坐标", Qt::CaseInsensitive)) {
        userMessage = QString("数据格式错误：%1\n\n坐标数据格式不正确。\n"
                             "请确保：\n"
                             "• 经度范围：-180 到 180\n"
                             "• 纬度范围：-90 到 90\n"
                             "• 坐标为数字格式\n\n"
                             "建议：检查Excel文件中的经纬度列数据。").arg(fileName);
    } else if (issue.contains("time", Qt::CaseInsensitive) || issue.contains("时间", Qt::CaseInsensitive)) {
        userMessage = QString("时间格式错误：%1\n\n时间数据格式不正确。\n"
                             "支持的时间格式：\n"
                             "• yyyy-MM-dd hh:mm:ss\n"
                             "• yyyy/MM/dd hh:mm:ss\n"
                             "• yyyy年MM月dd日 hh:mm:ss\n\n"
                             "建议：检查Excel文件中的时间列格式。").arg(fileName);
    } else if (issue.contains("empty", Qt::CaseInsensitive) || issue.contains("空", Qt::CaseInsensitive)) {
        userMessage = QString("文件内容错误：%1\n\n文件为空或没有有效数据。\n"
                             "请确保：\n"
                             "• 文件包含数据行（除表头外）\n"
                             "• 数据行不全为空\n"
                             "• 必填字段有值\n\n"
                             "建议：检查Excel文件是否包含车辆轨迹数据。").arg(fileName);
    } else {
        userMessage = QString("数据格式错误：%1\n\n%2\n\n"
                             "建议：\n"
                             "• 检查文件是否为标准Excel格式(.xlsx或.xls)\n"
                             "• 确认数据格式符合要求\n"
                             "• 尝试重新保存文件").arg(fileName).arg(issue);
    }
    
    qWarning() << "Data format error in file:" << fileName << "Issue:" << issue;
    return userMessage;
}

QString ErrorHandler::handleCoordinateConversionError(const QString& details)
{
    QString userMessage = QString("坐标转换错误\n\n%1\n\n"
                                 "可能的原因：\n"
                                 "• 坐标数据超出有效范围\n"
                                 "• 坐标格式不正确\n"
                                 "• 坐标系转换算法异常\n\n"
                                 "建议：\n"
                                 "• 检查原始坐标数据\n"
                                 "• 尝试关闭坐标转换功能\n"
                                 "• 联系技术支持").arg(details);
    
    qWarning() << "Coordinate conversion error:" << details;
    return userMessage;
}

QString ErrorHandler::handleValidationError(const QString& field, const QString& value, const QString& expected)
{
    QString userMessage;
    
    if (field.contains("车牌号", Qt::CaseInsensitive) || field.contains("plate", Qt::CaseInsensitive)) {
        userMessage = QString("车牌号验证失败\n\n"
                             "输入值：%1\n"
                             "要求：%2\n\n"
                             "请输入正确的车牌号格式。").arg(value).arg(expected);
    } else if (field.contains("经度", Qt::CaseInsensitive) || field.contains("longitude", Qt::CaseInsensitive)) {
        userMessage = QString("经度数据验证失败\n\n"
                             "输入值：%1\n"
                             "有效范围：-180 到 180\n\n"
                             "请检查经度数据是否正确。").arg(value);
    } else if (field.contains("纬度", Qt::CaseInsensitive) || field.contains("latitude", Qt::CaseInsensitive)) {
        userMessage = QString("纬度数据验证失败\n\n"
                             "输入值：%1\n"
                             "有效范围：-90 到 90\n\n"
                             "请检查纬度数据是否正确。").arg(value);
    } else if (field.contains("速度", Qt::CaseInsensitive) || field.contains("speed", Qt::CaseInsensitive)) {
        userMessage = QString("速度数据验证失败\n\n"
                             "输入值：%1\n"
                             "要求：非负数值\n\n"
                             "请检查速度数据是否为有效的数字。").arg(value);
    } else if (field.contains("方向", Qt::CaseInsensitive) || field.contains("direction", Qt::CaseInsensitive)) {
        userMessage = QString("方向数据验证失败\n\n"
                             "输入值：%1\n"
                             "有效范围：0 到 360 度\n\n"
                             "请检查方向数据是否正确。").arg(value);
    } else {
        userMessage = QString("数据验证失败\n\n"
                             "字段：%1\n"
                             "输入值：%2\n"
                             "要求：%3\n\n"
                             "请检查数据格式是否正确。").arg(field).arg(value).arg(expected);
    }
    
    qWarning() << "Validation error - Field:" << field << "Value:" << value << "Expected:" << expected;
    return userMessage;
}

QString ErrorHandler::handleNetworkError(const QString& service, const QString& details)
{
    QString userMessage;
    
    if (service.contains("map", Qt::CaseInsensitive) || service.contains("地图", Qt::CaseInsensitive)) {
        userMessage = QString("地图服务连接失败\n\n"
                             "错误详情：%1\n\n"
                             "可能的原因：\n"
                             "• 网络连接不稳定\n"
                             "• 地图服务暂时不可用\n"
                             "• 防火墙阻止了连接\n\n"
                             "建议：\n"
                             "• 检查网络连接\n"
                             "• 稍后重试\n"
                             "• 使用离线地图功能").arg(details);
    } else {
        userMessage = QString("网络服务错误\n\n"
                             "服务：%1\n"
                             "错误详情：%2\n\n"
                             "建议：\n"
                             "• 检查网络连接\n"
                             "• 稍后重试\n"
                             "• 联系网络管理员").arg(service).arg(details);
    }
    
    qWarning() << "Network error - Service:" << service << "Details:" << details;
    return userMessage;
}

QString ErrorHandler::handleMemoryError(const QString& operation)
{
    QString userMessage = QString("内存不足错误\n\n"
                                 "操作：%1\n\n"
                                 "系统内存不足，无法完成操作。\n\n"
                                 "建议：\n"
                                 "• 关闭其他不必要的程序\n"
                                 "• 减少处理的数据量\n"
                                 "• 重启应用程序\n"
                                 "• 考虑升级系统内存").arg(operation);
    
    qCritical() << "Memory error during operation:" << operation;
    return userMessage;
}

QString ErrorHandler::handleSystemError(const QString& operation, const QString& details)
{
    QString userMessage = QString("系统错误\n\n"
                                 "操作：%1\n"
                                 "错误详情：%2\n\n"
                                 "系统遇到意外错误。\n\n"
                                 "建议：\n"
                                 "• 重启应用程序\n"
                                 "• 检查系统资源\n"
                                 "• 联系技术支持\n"
                                 "• 查看系统日志").arg(operation).arg(details);
    
    qCritical() << "System error - Operation:" << operation << "Details:" << details;
    return userMessage;
}

void ErrorHandler::reportError(const ErrorInfo& error)
{
    m_errorHistory.append(error);
    
    // 输出到日志
    QString logMessage = QString("[%1] %2 - %3: %4")
                        .arg(getSeverityString(error.severity))
                        .arg(getErrorTypeString(error.type))
                        .arg(error.component)
                        .arg(error.technicalMessage);
    
    switch (error.severity) {
        case Info:
            qInfo() << logMessage;
            break;
        case Warning:
            qWarning() << logMessage;
            break;
        case Error:
            qWarning() << logMessage;
            break;
        case Critical:
            qCritical() << logMessage;
            emit criticalErrorOccurred(error);
            break;
    }
    
    emit errorReported(error);
}

void ErrorHandler::reportError(ErrorType type, ErrorSeverity severity, 
                              const QString& technicalMessage, const QString& userMessage,
                              const QString& context, const QString& component)
{
    ErrorInfo error(type, severity, technicalMessage, userMessage, context, component);
    reportError(error);
}

QList<ErrorHandler::ErrorInfo> ErrorHandler::getErrorHistory() const
{
    return m_errorHistory;
}

QList<ErrorHandler::ErrorInfo> ErrorHandler::getErrorsByType(ErrorType type) const
{
    QList<ErrorInfo> result;
    for (const ErrorInfo& error : m_errorHistory) {
        if (error.type == type) {
            result.append(error);
        }
    }
    return result;
}

QList<ErrorHandler::ErrorInfo> ErrorHandler::getErrorsBySeverity(ErrorSeverity severity) const
{
    QList<ErrorInfo> result;
    for (const ErrorInfo& error : m_errorHistory) {
        if (error.severity == severity) {
            result.append(error);
        }
    }
    return result;
}

void ErrorHandler::clearErrorHistory()
{
    m_errorHistory.clear();
}

int ErrorHandler::getErrorCount(ErrorType type) const
{
    if (type == UnknownError) {
        return m_errorHistory.size();
    }
    
    int count = 0;
    for (const ErrorInfo& error : m_errorHistory) {
        if (error.type == type) {
            count++;
        }
    }
    return count;
}

bool ErrorHandler::hasErrors() const
{
    return !m_errorHistory.isEmpty();
}

bool ErrorHandler::hasCriticalErrors() const
{
    for (const ErrorInfo& error : m_errorHistory) {
        if (error.severity == Critical) {
            return true;
        }
    }
    return false;
}

QString ErrorHandler::generateUserFriendlyMessage(ErrorType type, const QString& context)
{
    switch (type) {
        case FileAccessError:
            return QString("文件访问错误：%1").arg(context);
        case DataFormatError:
            return QString("数据格式错误：%1").arg(context);
        case CoordinateConversionError:
            return QString("坐标转换错误：%1").arg(context);
        case NetworkError:
            return QString("网络连接错误：%1").arg(context);
        case MemoryError:
            return QString("内存不足：%1").arg(context);
        case ValidationError:
            return QString("数据验证错误：%1").arg(context);
        case SystemError:
            return QString("系统错误：%1").arg(context);
        default:
            return QString("未知错误：%1").arg(context);
    }
}

QString ErrorHandler::getErrorTypeString(ErrorType type)
{
    switch (type) {
        case FileAccessError: return "文件访问错误";
        case DataFormatError: return "数据格式错误";
        case CoordinateConversionError: return "坐标转换错误";
        case NetworkError: return "网络错误";
        case MemoryError: return "内存错误";
        case ValidationError: return "数据验证错误";
        case SystemError: return "系统错误";
        default: return "未知错误";
    }
}

QString ErrorHandler::getSeverityString(ErrorSeverity severity)
{
    switch (severity) {
        case Info: return "信息";
        case Warning: return "警告";
        case Error: return "错误";
        case Critical: return "严重错误";
        default: return "未知";
    }
}