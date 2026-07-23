#ifndef LICENSEGUARD_H
#define LICENSEGUARD_H

#include <QString>

/// 软件运行时长许可：用网络时间校验是否允许启动
class LicenseGuard
{
public:
    struct Result {
        bool allowed = false;
        QString message;
    };

    /// 拉取网络时间并与到期日比较；无法获取网络时间时拒绝启动
    static Result verify();

private:
    LicenseGuard() = delete;
};

#endif // LICENSEGUARD_H
