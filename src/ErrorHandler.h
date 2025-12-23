#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QObject>
#include <QString>
#include <QDateTime>

/**
 * @class ErrorHandler
 * @brief 统一的错误处理和用户友好消息管理系统
 * 
 * ErrorHandler类提供统一的错误分类、记录和用户友好消息生成功能。
 * 支持不同类型的错误处理和本地化消息。
 */
class ErrorHandler : public QObject
{
    Q_OBJECT
    
public:
    enum ErrorType {
        FileAccessError,        // 文件访问错误
        DataFormatError,        // 数据格式错误
        CoordinateConversionError, // 坐标转换错误
        NetworkError,           // 网络错误
        MemoryError,           // 内存错误
        ValidationError,       // 数据验证错误
        SystemError,           // 系统错误
        UnknownError           // 未知错误
    };
    Q_ENUM(ErrorType)
    
    enum ErrorSeverity {
        Info,                  // 信息
        Warning,               // 警告
        Error,                 // 错误
        Critical               // 严重错误
    };
    Q_ENUM(ErrorSeverity)
    
    struct ErrorInfo {
        ErrorType type;
        ErrorSeverity severity;
        QString technicalMessage;  // 技术详细信息
        QString userMessage;       // 用户友好消息
        QString context;           // 错误上下文
        QDateTime timestamp;
        QString component;         // 发生错误的组件
        
        ErrorInfo() : type(UnknownError), severity(Error), timestamp(QDateTime::currentDateTime()) {}
        
        ErrorInfo(ErrorType t, ErrorSeverity s, const QString& tech, const QString& user, 
                 const QString& ctx = QString(), const QString& comp = QString())
            : type(t), severity(s), technicalMessage(tech), userMessage(user), 
              context(ctx), timestamp(QDateTime::currentDateTime()), component(comp) {}
    };
    
    explicit ErrorHandler(QObject *parent = nullptr);
    
    // 静态方法用于快速错误处理
    static QString handleFileAccessError(const QString& filePath, const QString& operation);
    static QString handleDataFormatError(const QString& fileName, const QString& issue);
    static QString handleCoordinateConversionError(const QString& details);
    static QString handleValidationError(const QString& field, const QString& value, const QString& expected);
    static QString handleNetworkError(const QString& service, const QString& details);
    static QString handleMemoryError(const QString& operation);
    static QString handleSystemError(const QString& operation, const QString& details);
    
    // 实例方法用于详细错误管理
    void reportError(const ErrorInfo& error);
    void reportError(ErrorType type, ErrorSeverity severity, 
                    const QString& technicalMessage, const QString& userMessage,
                    const QString& context = QString(), const QString& component = QString());
    
    // 获取错误历史
    QList<ErrorInfo> getErrorHistory() const;
    QList<ErrorInfo> getErrorsByType(ErrorType type) const;
    QList<ErrorInfo> getErrorsBySeverity(ErrorSeverity severity) const;
    void clearErrorHistory();
    
    // 错误统计
    int getErrorCount(ErrorType type = UnknownError) const;
    bool hasErrors() const;
    bool hasCriticalErrors() const;
    
signals:
    void errorReported(const ErrorInfo& error);
    void criticalErrorOccurred(const ErrorInfo& error);
    
private:
    QList<ErrorInfo> m_errorHistory;
    
    // 生成用户友好消息的辅助方法
    static QString generateUserFriendlyMessage(ErrorType type, const QString& context);
    static QString getErrorTypeString(ErrorType type);
    static QString getSeverityString(ErrorSeverity severity);
};

// 便利宏定义
#define HANDLE_FILE_ERROR(filePath, operation) \
    ErrorHandler::handleFileAccessError(filePath, operation)

#define HANDLE_DATA_ERROR(fileName, issue) \
    ErrorHandler::handleDataFormatError(fileName, issue)

#define HANDLE_COORD_ERROR(details) \
    ErrorHandler::handleCoordinateConversionError(details)

#define HANDLE_VALIDATION_ERROR(field, value, expected) \
    ErrorHandler::handleValidationError(field, value, expected)

#define HANDLE_NETWORK_ERROR(service, details) \
    ErrorHandler::handleNetworkError(service, details)

#define HANDLE_MEMORY_ERROR(operation) \
    ErrorHandler::handleMemoryError(operation)

#define HANDLE_SYSTEM_ERROR(operation, details) \
    ErrorHandler::handleSystemError(operation, details)

#endif // ERRORHANDLER_H