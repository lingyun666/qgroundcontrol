#include "tcpserver.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDataStream>

// 定义命令常量
const QByteArray TcpServer::CMD_LIST = "LIST";
const QByteArray TcpServer::CMD_GET = "GET";

TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_shareDir(QDir::currentPath() + "/shared_files")
{
    // 创建共享目录
    QDir dir;
    if (!dir.exists(m_shareDir)) {
        dir.mkpath(m_shareDir);
    }

    // 连接信号和槽
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    stopServer();
    
    // 清理所有传输
    for (auto it = m_transfers.begin(); it != m_transfers.end(); ++it) {
        if (it.value()->file) {
            it.value()->file->close();
            delete it.value()->file;
        }
        delete it.value();
    }
    m_transfers.clear();
}

bool TcpServer::startServer(quint16 port)
{
    qDebug() << "正在启动服务器, 端口:" << port;

    if (m_server->isListening()) {
        qDebug() << "服务器已在运行，先停止当前监听";
        m_server->close();
    }
    
    // 开始监听
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit error(QString("服务器启动失败: %1").arg(m_server->errorString()));
        qDebug() << "服务器启动失败:" << m_server->errorString();
        emit serverStatus(false);
        return false;
    }
    
    qDebug() << "服务器已成功启动，监听端口:" << port;
    emit serverStatus(true);
    return true;
}

void TcpServer::stopServer()
{
    qDebug() << "正在停止服务器...";
    
    // 关闭所有客户端连接
    for (QTcpSocket *socket : m_clients) {
        socket->disconnectFromHost();
    }
    
    // 停止监听
    m_server->close();
    
    qDebug() << "服务器已停止";
    emit serverStatus(false);
}

bool TcpServer::isListening() const
{
    return m_server->isListening();
}

void TcpServer::setShareDirectory(const QString &dir)
{
    qDebug() << "设置共享目录:" << dir << ", 该目录存在状态:" << QDir(dir).exists();
    m_shareDir = dir;
    // 创建共享目录
    QDir directory;
    if (!directory.exists(m_shareDir)) {
        directory.mkpath(m_shareDir);
    }
}

QList<QPair<QString, qint64>> TcpServer::getFileList() const
{
    QList<QPair<QString, qint64>> fileList;
    
    QDir dir(m_shareDir);
    QFileInfoList infoList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    
    for (const QFileInfo &fileInfo : infoList) {
        fileList.append(qMakePair(fileInfo.fileName(), fileInfo.size()));
    }
    
    return fileList;
}

void TcpServer::onNewConnection()
{
    QTcpSocket *clientSocket = m_server->nextPendingConnection();
    
    if (!clientSocket) {
        return;
    }

    // 保存客户端连接
    m_clients.append(clientSocket);
    
    // 连接客户端socket信号
    connect(clientSocket, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
    connect(clientSocket, &QTcpSocket::bytesWritten, this, &TcpServer::onBytesWritten);
    connect(clientSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &TcpServer::onError);

    // 发送客户端连接信号
    QString clientIP = clientSocket->peerAddress().toString();
    emit newConnection(clientIP);
    qDebug() << "新客户端连接:" << clientIP;
}

void TcpServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        return;
    }

    QByteArray data = clientSocket->readAll();
    parseCommand(data, clientSocket);
}

void TcpServer::parseCommand(const QByteArray &data, QTcpSocket *socket)
{
    if (data.startsWith(CMD_LIST)) {
        // 客户端请求文件列表
        qDebug() << "客户端请求文件列表";
        sendFileList(socket);
    }
    else if (data.startsWith(CMD_GET)) {
        // 客户端请求下载文件
        QString fileName = QString::fromUtf8(data.mid(CMD_GET.length() + 1));
        qDebug() << "客户端请求下载文件:" << fileName;
        emit clientRequest(fileName);
        sendFile(socket, fileName);
    }
    else {
        qDebug() << "收到未知命令:" << data;
    }
}

void TcpServer::sendFileList(QTcpSocket *socket)
{
    QList<QPair<QString, qint64>> fileList = getFileList();
    
    // 构建JSON数据
    QJsonArray jsonArray;
    for (const auto &pair : fileList) {
        QJsonObject fileObj;
        fileObj["name"] = pair.first;
        fileObj["size"] = QString::number(pair.second);
        jsonArray.append(fileObj);
    }
    
    QJsonObject listObj;
    listObj["files"] = jsonArray;
    listObj["count"] = fileList.size();
    
    QJsonDocument doc(listObj);
    QByteArray listData = doc.toJson();
    
    // 发送文件列表
    socket->write(listData);
}

void TcpServer::sendFile(QTcpSocket *socket, const QString &fileName)
{
    qDebug() << "请求发送文件:" << fileName << "到客户端:" << socket->peerAddress().toString();
    
    // 检查是否已经在传输文件
    if (m_transfers.contains(socket)) {
        qDebug() << "已有文件正在传输给该客户端，无法同时发送多个文件";
        QJsonObject errorInfo;
        errorInfo["error"] = "已有文件正在传输中";
        QJsonDocument doc(errorInfo);
        QByteArray errorData = doc.toJson();
        socket->write(errorData);
        return;
    }
    
    // 构建文件路径
    QString filePath = m_shareDir + "/" + fileName;
    qDebug() << "尝试发送文件路径:" << filePath;
    
    // 打开文件
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件:" << filePath << "错误:" << file->errorString();
        QJsonObject errorInfo;
        errorInfo["error"] = QString("文件不存在或无法访问: %1").arg(fileName);
        QJsonDocument doc(errorInfo);
        QByteArray errorData = doc.toJson();
        socket->write(errorData);
        
        delete file;
        return;
    }
    
    // 创建传输记录
    FileTransfer *transfer = new FileTransfer;
    transfer->file = file;
    transfer->fileName = fileName;
    transfer->fileSize = file->size();
    transfer->bytesSent = 0;
    transfer->headerSent = false;
    m_transfers[socket] = transfer;
    
    qDebug() << "准备发送文件:" << fileName << "大小:" << transfer->fileSize << "字节";
    
    // 发送文件头信息
    QByteArray header = prepareFileHeader(fileName, transfer->fileSize);
    qint64 bytesWritten = socket->write(header);
    if (bytesWritten <= 0) {
        qDebug() << "发送文件头失败:" << socket->errorString();
        file->close();
        delete file;
        delete transfer;
        m_transfers.remove(socket);
        return;
    }
    
    qDebug() << "已发送文件头:" << bytesWritten << "字节";
    transfer->headerSent = true;
    transfer->bytesSent = bytesWritten;
    
    // 文件头发送后，在onBytesWritten中继续发送文件数据
}

QByteArray TcpServer::prepareFileHeader(const QString &fileName, qint64 fileSize)
{
    QJsonObject fileInfo;
    fileInfo["fileName"] = fileName;
    fileInfo["fileSize"] = QString::number(fileSize);
    fileInfo["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(fileInfo);
    QByteArray jsonData = doc.toJson();
    
    // 确保文件头信息不超过HEADER_SIZE
    if (jsonData.size() > HEADER_SIZE - 8) {
        qDebug() << "警告: 文件头信息已被截断";
        jsonData.truncate(HEADER_SIZE - 8);
    }
    
    // 创建新的头部，前4字节保存JSON数据的长度
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    // 设置数据流的版本，确保客户端和服务器使用相同的格式
    stream.setVersion(QDataStream::Qt_6_0);
    stream << (qint32)jsonData.size();
    
    // 添加JSON数据
    header.append(jsonData);
    
    // 填充到固定大小
    header = header.leftJustified(HEADER_SIZE, '\0', true);
    
    qDebug() << "发送文件头: 大小=" << jsonData.size() 
             << ", 总长度=" << header.size() 
             << ", 文件名=" << fileName;
    
    return header;
}

void TcpServer::onDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        return;
    }
    
    qDebug() << "客户端断开连接:" << clientSocket->peerAddress().toString();
    
    // 清理资源
    if (m_transfers.contains(clientSocket)) {
        FileTransfer *transfer = m_transfers[clientSocket];
        if (transfer->file) {
            transfer->file->close();
            delete transfer->file;
        }
        delete transfer;
        m_transfers.remove(clientSocket);
    }
    
    // 从客户端列表中移除
    m_clients.removeOne(clientSocket);
    clientSocket->deleteLater();
}

void TcpServer::onBytesWritten(qint64 bytes)
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_transfers.contains(socket)) {
        return;
    }

    FileTransfer *transfer = m_transfers[socket];
    
    // 如果文件头已发送，就继续发送文件数据
    if (transfer->headerSent) {
        transfer->bytesSent += bytes;
        
        // 计算和发送进度
        int progress = static_cast<int>((transfer->bytesSent * 100) / (transfer->fileSize + HEADER_SIZE));
        emit sendProgress(transfer->fileName, progress);
        
        qDebug() << "文件发送进度:" << transfer->fileName << ", " << progress << "%, 已发送:" 
                << transfer->bytesSent - HEADER_SIZE << "/" << transfer->fileSize << "字节";
        
        // 如果还有数据要发送
        if (transfer->file->bytesAvailable() > 0) {
            // 每次发送的数据块大小 (64KB)
            const qint64 blockSize = 64 * 1024;
            QByteArray block = transfer->file->read(qMin(blockSize, transfer->file->bytesAvailable()));
            
            qint64 bytesWritten = socket->write(block);
            if (bytesWritten <= 0) {
                emit error(QString("发送文件数据时出错: %1").arg(socket->errorString()));
                qDebug() << "发送文件数据时出错:" << socket->errorString();
            }
        } 
        // 如果所有数据都已发送
        else if (transfer->bytesSent >= transfer->fileSize + HEADER_SIZE) {
            qDebug() << "文件发送完成:" << transfer->fileName 
                    << ", 大小:" << transfer->fileSize 
                    << ", 发送:" << transfer->bytesSent - HEADER_SIZE << "字节";
            emit fileSent(transfer->fileName, transfer->fileSize);
            
            // 清理资源
            transfer->file->close();
            delete transfer->file;
            delete transfer;
            m_transfers.remove(socket);
        }
    }
}

void TcpServer::onError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) {
        return;
    }
    
    emit error(QString("Socket错误: %1").arg(clientSocket->errorString()));
    qDebug() << "Socket错误:" << socketError << clientSocket->errorString();
    
    // 清理资源
    if (m_transfers.contains(clientSocket)) {
        FileTransfer *transfer = m_transfers[clientSocket];
        if (transfer->file) {
            transfer->file->close();
            delete transfer->file;
        }
        delete transfer;
        m_transfers.remove(clientSocket);
    }
} 