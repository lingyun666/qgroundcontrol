/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QByteArray>
#include <QMutex>

/**
 * @brief UDP通信类，用于发送和接收MAVLink命令
 */
class UdpCommandLink : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 创建或获取UdpCommandLink的单例实例
     * @param remoteHost 远程主机地址
     * @param remotePort 远程主机端口
     * @param localPort 本地监听端口
     * @return UdpCommandLink实例
     */
    static UdpCommandLink* instance(const QString& remoteHost = QString(), quint16 remotePort = 0, quint16 localPort = 0);

    /**
     * @brief 构造函数
     * @param remoteHost 远程主机地址
     * @param remotePort 远程主机端口
     * @param localPort 本地监听端口
     * @param parent 父对象
     */
    explicit UdpCommandLink(const QString& remoteHost = QString(), quint16 remotePort = 0, quint16 localPort = 0, QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~UdpCommandLink();

    /**
     * @brief 通过UDP发送命令
     * @param data 要发送的数据
     * @return 成功发送的字节数，-1表示发送失败
     */
    qint64 sendCommand(const QByteArray &data);

    /**
     * @brief 设置远程主机地址
     * @param address 远程主机地址
     */
    void setRemoteAddress(const QString &address);

    /**
     * @brief 设置远程主机端口
     * @param port 远程主机端口
     */
    void setRemotePort(quint16 port);

    /**
     * @brief 设置本地监听端口
     * @param port 本地监听端口
     */
    void setLocalPort(quint16 port);

    /**
     * @brief 获取当前本地监听端口
     * @return 本地监听端口
     */
    quint16 getLocalPort() const { return _localPort; }

    /**
     * @brief 初始化UDP连接
     * @return 是否成功初始化
     */
    bool initialize();

signals:
    /**
     * @brief 接收到数据时发出的信号
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

private slots:
    /**
     * @brief 处理接收到的UDP数据包
     */
    void _readPendingDatagrams();

private:
    // 禁止拷贝构造
    UdpCommandLink(const UdpCommandLink&) = delete;
    UdpCommandLink& operator=(const UdpCommandLink&) = delete;
    
    static UdpCommandLink* _instance; // 单例实例
    static QMutex _mutex;             // 互斥锁，保护单例创建
    
    QUdpSocket* _socket;              // UDP套接字
    QHostAddress _remoteAddress;      // 远程主机地址
    quint16 _remotePort;              // 远程主机端口
    quint16 _localPort;               // 本地监听端口
    bool _isInitialized;              // 是否已初始化
}; 