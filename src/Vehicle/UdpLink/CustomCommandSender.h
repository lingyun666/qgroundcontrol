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
#include <QByteArray>
#include "UdpCommandLink.h"
#include "QGCMAVLink.h"
#include "MAVLinkProtocol.h"

/**
 * @brief 自定义命令发送器，用于在QML中直接发送命令而无需依赖Vehicle对象
 * 此类可以在无人机未连接时仍能发送UDP命令
 */
class CustomCommandSender : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString remoteAddress READ remoteAddress WRITE setRemoteAddress NOTIFY remoteAddressChanged)
    Q_PROPERTY(int remotePort READ remotePort WRITE setRemotePort NOTIFY remotePortChanged)
    Q_PROPERTY(int localPort READ localPort WRITE setLocalPort NOTIFY localPortChanged)
    
public:
    explicit CustomCommandSender(QObject *parent = nullptr);
    ~CustomCommandSender();
    
    /**
     * @brief 发送MAVLink命令
     * @param componentId 目标组件ID
     * @param command 命令ID
     * @param showError 是否显示错误
     * @param param1-param7 命令参数
     * @return 是否成功发送
     */
    Q_INVOKABLE bool sendCommandByUdp(int componentId, int command, bool showError, 
                                     float param1 = 0, float param2 = 0, float param3 = 0,
                                     float param4 = 0, float param5 = 0, float param6 = 0, float param7 = 0);
    
    /**
     * @brief 获取远程地址
     */
    QString remoteAddress() const;
    
    /**
     * @brief 设置远程地址
     */
    void setRemoteAddress(const QString &address);
    
    /**
     * @brief 获取远程端口
     */
    int remotePort() const;
    
    /**
     * @brief 设置远程端口
     */
    void setRemotePort(int port);
    
    /**
     * @brief 获取本地端口
     */
    int localPort() const;
    
    /**
     * @brief 设置本地端口
     */
    void setLocalPort(int port);
    
signals:
    void remoteAddressChanged();
    void remotePortChanged();
    void localPortChanged();
    void commandSent(bool success);
    void dataReceived(const QByteArray &data);
    
private:
    UdpCommandLink* _udpLink;
    QString _remoteAddress;
    int _remotePort;
    int _localPort;
    int _systemId = 255;  // 默认系统ID，可以根据需要修改
}; 