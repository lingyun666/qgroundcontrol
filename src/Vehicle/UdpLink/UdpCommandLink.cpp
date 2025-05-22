/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "UdpCommandLink.h"
#include <QDebug>

// 静态成员初始化
UdpCommandLink* UdpCommandLink::_instance = nullptr;
QMutex UdpCommandLink::_mutex;

UdpCommandLink::UdpCommandLink(const QString& remoteHost, quint16 remotePort, quint16 localPort, QObject *parent) : 
    QObject(parent),
    _socket(nullptr),
    _remoteAddress(remoteHost),  // 直接使用传入的参数
    _remotePort(remotePort),     // 直接使用传入的参数
    _localPort(localPort),       // 直接使用传入的参数
    _isInitialized(false)
{
    // 如果未指定参数，会在实例化时由CustomCommandSender提供
    if (remoteHost.isEmpty() || remotePort == 0 || localPort == 0) {
        qDebug() << "UdpCommandLink: 构造函数 - 部分参数未指定，将使用创建者提供的默认值";
    } else {
        qDebug() << "UdpCommandLink: 构造函数 - 使用指定参数：" << remoteHost << ":" << remotePort << ", 本地端口:" << localPort;
    }
}

UdpCommandLink::~UdpCommandLink()
{
    if (_socket) {
        _socket->close();
        delete _socket;
        _socket = nullptr;
    }
    qDebug() << "UdpCommandLink: 析构函数";
}

UdpCommandLink* UdpCommandLink::instance(const QString& remoteHost, quint16 remotePort, quint16 localPort)
{
    if (_instance == nullptr) {
        _mutex.lock();
        if (_instance == nullptr) {
            _instance = new UdpCommandLink(remoteHost, remotePort, localPort);
            _instance->initialize();
        } else if (!remoteHost.isEmpty() || remotePort != 0 || localPort != 0) {
            // 已存在实例但收到了新的配置，更新现有实例
            if (!remoteHost.isEmpty()) {
                _instance->setRemoteAddress(remoteHost);
            }
            if (remotePort != 0) {
                _instance->setRemotePort(remotePort);
            }
            if (localPort != 0) {
                _instance->setLocalPort(localPort);
            }
        }
        _mutex.unlock();
    }
    return _instance;
}

bool UdpCommandLink::initialize()
{
    if (_isInitialized) {
        return true;
    }

    // 检查参数是否完整
    if (_remoteAddress.isNull() || _remotePort == 0 || _localPort == 0) {
        qWarning() << "UdpCommandLink: 初始化失败 - 网络参数不完整";
        return false;
    }

    if (_socket) {
        delete _socket;
    }

    _socket = new QUdpSocket(this);
    
    // 绑定本地端口，用于接收数据
    if (!_socket->bind(QHostAddress::Any, _localPort)) {
        qWarning() << "UdpCommandLink: 无法绑定到端口" << _localPort << ":" << _socket->errorString();
        delete _socket;
        _socket = nullptr;
        return false;
    }
    
    // 连接信号槽，处理接收到的数据
    connect(_socket, &QUdpSocket::readyRead, this, &UdpCommandLink::_readPendingDatagrams);
    
    qDebug() << "UdpCommandLink: 初始化成功，监听端口:" << _localPort;
    qDebug() << "UdpCommandLink: 远程地址:" << _remoteAddress.toString() << "端口:" << _remotePort;
    
    _isInitialized = true;
    return true;
}

qint64 UdpCommandLink::sendCommand(const QByteArray &data)
{
    if (!_socket || !_isInitialized) {
        qWarning() << "UdpCommandLink: 尝试在未初始化的套接字上发送数据";
        return -1;
    }
    
    qint64 bytesSent = _socket->writeDatagram(data, _remoteAddress, _remotePort);
    
    if (bytesSent == -1) {
        qWarning() << "UdpCommandLink: 发送数据失败:" << _socket->errorString();
    } else {
        qDebug() << "UdpCommandLink: 发送" << bytesSent << "字节到" << _remoteAddress.toString() << ":" << _remotePort;
    }
    
    return bytesSent;
}

void UdpCommandLink::_readPendingDatagrams()
{
    while (_socket && _socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(_socket->pendingDatagramSize());
        
        QHostAddress sender;
        quint16 senderPort;
        
        _socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        
        qDebug() << "UdpCommandLink: 接收到" << datagram.size() << "字节，来自" << sender.toString() << ":" << senderPort;
        
        // 发出信号，通知接收到数据
        emit dataReceived(datagram);
    }
}

void UdpCommandLink::setRemoteAddress(const QString &address)
{
    _remoteAddress = QHostAddress(address);
    qDebug() << "UdpCommandLink: 设置远程地址为" << address;
}

void UdpCommandLink::setRemotePort(quint16 port)
{
    _remotePort = port;
    qDebug() << "UdpCommandLink: 设置远程端口为" << port;
}

void UdpCommandLink::setLocalPort(quint16 port)
{
    // 如果已经初始化，需要重新初始化以使用新端口
    bool wasInitialized = _isInitialized;
    
    if (wasInitialized) {
        if (_socket) {
            _socket->close();
            delete _socket;
            _socket = nullptr;
        }
        _isInitialized = false;
    }
    
    _localPort = port;
    qDebug() << "UdpCommandLink: 设置本地端口为" << port;
    
    if (wasInitialized) {
        initialize();
    }
} 