#include "Core/LicenseGuard.h"

#include "Core/AppLogger.h"

#include <QDate>
#include <QDateTime>
#include <QHostInfo>
#include <QTimeZone>
#include <QUdpSocket>
#include <QtEndian>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

constexpr int kNtpTimeoutMs = 3000;
// NTP 纪元 (1900-01-01) 与 Unix 纪元 (1970-01-01) 相差秒数
constexpr qint64 kNtpUnixEpochDelta = 2208988800LL;

QDateTime parseNtpTimestamp(const QByteArray& packet)
{
    if (packet.size() < 48)
        return {};

    // 使用服务器 Transmit Timestamp（偏移 40）
    const auto seconds = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(packet.constData() + 40));
    const auto fraction = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(packet.constData() + 44));

    if (seconds == 0)
        return {};

    const qint64 unixSecs = qint64(seconds) - kNtpUnixEpochDelta;
    const qint64 unixMs = unixSecs * 1000
        + qint64((quint64(fraction) * 1000ULL) >> 32);
    return QDateTime::fromMSecsSinceEpoch(unixMs, QTimeZone::UTC);
}

QDateTime fetchNtpUtcFromHost(const QString& host)
{
    const QHostInfo info = QHostInfo::fromName(host);
    if (info.addresses().isEmpty()) {
        AppLogger::warn(QStringLiteral("许可校验：NTP 域名解析失败 %1").arg(host));
        return {};
    }

    QByteArray request(48, char(0));
    // LI=0, VN=3 (NTPv3), Mode=3 (client) => 0x1B
    request[0] = char(0x1B);

    for (const QHostAddress& address : info.addresses()) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol
            && address.protocol() != QAbstractSocket::IPv6Protocol) {
            continue;
        }

        QUdpSocket socket;
        if (socket.writeDatagram(request, address, 123) < 0) {
            AppLogger::warn(QStringLiteral("许可校验：NTP 发送失败 %1 (%2) | %3")
                                .arg(host, address.toString(), socket.errorString()));
            continue;
        }

        if (!socket.waitForReadyRead(kNtpTimeoutMs)) {
            AppLogger::warn(QStringLiteral("许可校验：NTP 超时 %1 (%2)")
                                .arg(host, address.toString()));
            continue;
        }

        QByteArray response;
        response.resize(int(socket.pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort = 0;
        if (socket.readDatagram(response.data(), response.size(), &sender, &senderPort) < 48) {
            AppLogger::warn(QStringLiteral("许可校验：NTP 响应过短 %1").arg(host));
            continue;
        }

        const QDateTime utc = parseNtpTimestamp(response);
        if (utc.isValid())
            return utc;

        AppLogger::warn(QStringLiteral("许可校验：NTP 时间戳无效 %1").arg(host));
    }

    return {};
}

QDateTime fetchNetworkUtc()
{
    // 国家授时中心及相关国内公共 NTP（UDP 123）
    const QStringList ntpHosts = {
        QStringLiteral("ntp1.ntsc.ac.cn"),
        QStringLiteral("ntp2.ntsc.ac.cn"),
        QStringLiteral("ntp3.ntsc.ac.cn"),
        QStringLiteral("ntp1.aliyun.com"),
        QStringLiteral("ntp2.aliyun.com"),
        QStringLiteral("ntp3.aliyun.com"),
        QStringLiteral("ntp1.cstnet.cn"),
        QStringLiteral("ntp2.cstnet.cn"),
    };

    for (const QString& host : ntpHosts) {
        const QDateTime utc = fetchNtpUtcFromHost(host);
        if (utc.isValid()) {
            AppLogger::info(QStringLiteral("许可校验：NTP 网络时间来源 %1 -> %2")
                                .arg(host,
                                     utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                         + QStringLiteral(" UTC")));
            return utc;
        }
    }

    return {};
}

void showLicenseDeniedMessage(const QString& message)
{
    AppLogger::critical(message);
    AppLogger::flush();
#ifdef Q_OS_WIN
    MessageBoxW(nullptr,
                reinterpret_cast<LPCWSTR>(message.utf16()),
                L"CarMove 运行许可",
                MB_OK | MB_ICONERROR | MB_TOPMOST);
#else
    Q_UNUSED(message);
#endif
}

} // namespace

LicenseGuard::Result LicenseGuard::verify()
{
    Result result;

    // 运行许可到期日写死在代码中（含当天有效）。注意：6 月无 31 日，按 2026-06-30。
    const QDate expireDate(2026, 7, 31);

    const QDateTime networkUtc = fetchNetworkUtc();
    if (!networkUtc.isValid()) {
        result.allowed = false;
        result.message = QStringLiteral("无法获取网络时间，软件无法启动。\n请检查网络连接后重试。");
        showLicenseDeniedMessage(result.message);
        return result;
    }

    const QDate networkDate = networkUtc.toTimeZone(QTimeZone(QByteArrayLiteral("Asia/Shanghai"))).date();
    AppLogger::info(QStringLiteral("许可校验：网络日期=%1，到期日=%2")
                        .arg(networkDate.toString(QStringLiteral("yyyy-MM-dd")),
                             expireDate.toString(QStringLiteral("yyyy-MM-dd"))));

    if (networkDate > expireDate) {
        result.allowed = false;
        result.message = QStringLiteral("软件运行许可已过期（到期日 %1）。\n当前网络日期 %2，请联系管理员续期。")
                             .arg(expireDate.toString(QStringLiteral("yyyy-MM-dd")),
                                  networkDate.toString(QStringLiteral("yyyy-MM-dd")));
        showLicenseDeniedMessage(result.message);
        return result;
    }

    result.allowed = true;
    result.message = QStringLiteral("运行许可有效至 %1").arg(expireDate.toString(QStringLiteral("yyyy-MM-dd")));
    AppLogger::info(result.message);
    AppLogger::flush();
    return result;
}
