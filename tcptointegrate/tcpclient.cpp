#include "tcpclient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>

// 定义命令常量
const QByteArray TcpClient::CMD_LIST = "LIST";
const QByteArray TcpClient::CMD_GET = "GET";

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_file(nullptr)
    , m_fileSize(0)
    , m_bytesReceived(0)
    , m_receivingHeader(false)
    , m_receivingFileList(false)
{
    // 连接信号和槽
    connect(m_socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpClient::onBytesWritten);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &TcpClient::onError);
}

TcpClient::~TcpClient()
{
    disconnectFromServer();
}

bool TcpClient::connectToServer(const QString &host, quint16 port)
{
    qDebug() << "尝试连接到服务器:" << host << ", 端口:" << port;
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接到服务器，先断开连接";
        m_socket->disconnectFromHost();
        m_socket->waitForDisconnected();
    }
    
    // 连接服务器
    m_socket->connectToHost(host, port);
    return true;
}

void TcpClient::disconnectFromServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
        
        // 如果socket状态不是立即断开，则等待
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
    
    // 清理文件资源
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void TcpClient::requestFileList()
{
    qDebug() << "请求服务器文件列表, 当前连接状态:" << (m_socket->state() == QAbstractSocket::ConnectedState ? "已连接" : "未连接");
    
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return;
    }
    
    // 清空文件列表缓冲区
    m_fileListBuffer.clear();
    m_receivingFileList = true;
    
    // 发送请求文件列表命令
    m_socket->write(CMD_LIST);
}

bool TcpClient::downloadFile(const QString &fileName, const QString &savePath)
{
#ifdef Q_OS_ANDROID
    // 在Android上简单记录一下权限情况，但不阻止继续操作
    qDebug() << "正在Android平台下载文件 - 权限可能需要手动授予";
#endif

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return false;
    }

    // 检查是否已有下载中的文件
    if (m_file) {
        emit error("已有文件正在下载中");
        return false;
    }

    // 确保保存目录存在
    QFileInfo fileInfo(savePath);
    QDir saveDir = fileInfo.dir();
    
    // 记录详细日志
    qDebug() << "尝试下载文件: " << fileName << " 到路径: " << savePath;
    qDebug() << "目录信息: " << saveDir.path() << ", 存在状态: " << saveDir.exists();
    
    if (!saveDir.exists()) {
        qDebug() << "尝试创建目录: " << saveDir.path();
        if (!saveDir.mkpath(".")) {
            QString errorMsg = QString("无法创建目录: %1").arg(saveDir.path());
            qDebug() << errorMsg;
            emit error(errorMsg);
            return false;
        }
    }

    // 创建完整的保存路径
    QString fullPath = savePath + "/" + fileName;
    qDebug() << "文件将保存至: " << fullPath;
    
    m_file = new QFile(fullPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        QString errorMsg = QString("无法创建文件: %1 (错误: %2)").arg(fullPath, m_file->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        delete m_file;
        m_file = nullptr;
        return false;
    }

    // 初始化变量
    m_currentFileName = fileName;
    m_fileSize = 0;
    m_bytesReceived = 0;
    m_receivingHeader = true;
    m_headerBuffer.clear();

    // 发送下载文件的命令
    QByteArray command = CMD_GET + " " + fileName.toUtf8();
    qint64 bytesWritten = m_socket->write(command);
    
    if (bytesWritten <= 0) {
        QString errorMsg = QString("发送下载命令失败: %1").arg(m_socket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        m_file->close();
        delete m_file;
        m_file = nullptr;
        return false;
    }
    
    qDebug() << "发送下载命令: " << command;
    return true;
}

bool TcpClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpClient::onConnected()
{
    qDebug() << "已连接到服务器:" << m_socket->peerAddress().toString() << ":" << m_socket->peerPort();
    emit connectionStateChanged(true);
}

void TcpClient::onDisconnected()
{
    qDebug() << "与服务器断开连接";
    
    // 清理资源
    if (m_file) {
        // 如果正在下载文件但未完成，发出错误信号
        if (m_bytesReceived < m_fileSize) {
            emit error(QString("下载中断: %1 (已接收 %2/%3 字节)")
                       .arg(m_currentFileName)
                       .arg(m_bytesReceived)
                       .arg(m_fileSize));
        }
        
        m_file->close();
        delete m_file;
        m_file = nullptr;
        m_currentFileName.clear();
        m_fileSize = 0;
        m_bytesReceived = 0;
    }
    
    // 清理其它缓冲区
    m_headerBuffer.clear();
    m_fileListBuffer.clear();
    m_receivingHeader = false;
    m_receivingFileList = false;
    
    emit connectionStateChanged(false);
}

void TcpClient::onReadyRead()
{
    if (m_receivingFileList) {
        // 正在接收文件列表
        m_fileListBuffer.append(m_socket->readAll());
        processFileList(m_fileListBuffer);
    }
    else if (m_receivingHeader) {
        // 接收文件头
        m_headerBuffer.append(m_socket->readAll());
        
        // 检查是否接收到完整的文件头
        if (m_headerBuffer.size() >= HEADER_SIZE) {
            QByteArray headerData = m_headerBuffer.left(HEADER_SIZE);
            m_headerBuffer.remove(0, HEADER_SIZE);
            processFileHeader(headerData);
            
            // 如果有剩余数据，则属于文件内容
            if (!m_headerBuffer.isEmpty() && m_file) {
                m_file->write(m_headerBuffer);
                m_bytesReceived += m_headerBuffer.size();
                m_headerBuffer.clear();
                
                // 计算进度
                int progress = static_cast<int>((m_bytesReceived * 100) / m_fileSize);
                emit downloadProgress(m_currentFileName, progress);
                
                // 检查文件是否接收完成
                if (m_bytesReceived >= m_fileSize) {
                    finishFileDownload();
                }
            }
        }
    }
    else if (m_file) {
        // 接收文件内容
        QByteArray data = m_socket->readAll();
        qint64 bytesWritten = m_file->write(data);
        m_bytesReceived += bytesWritten;
        
        // 计算进度
        int progress = static_cast<int>((m_bytesReceived * 100) / m_fileSize);
        emit downloadProgress(m_currentFileName, progress);
        
        // 检查文件是否接收完成
        if (m_bytesReceived >= m_fileSize) {
            finishFileDownload();
        }
    }
    else {
        // 丢弃数据
        m_socket->readAll();
    }
}

void TcpClient::processFileHeader(const QByteArray &headerData)
{
    qDebug() << "处理文件头数据, 大小:" << headerData.size() << "字节";
    
    // 读取JSON数据长度（前4字节）
    QDataStream stream(headerData);
    // 设置数据流的版本，确保客户端和服务器使用相同的格式
    stream.setVersion(QDataStream::Qt_6_0);
    
    qint32 jsonSize;
    stream >> jsonSize;
    
    qDebug() << "接收到文件头，JSON大小:" << jsonSize 
             << ", 头部总大小:" << headerData.size();
    
    if (jsonSize <= 0 || jsonSize > headerData.size() - sizeof(qint32)) {
        emit error(QString("文件头信息格式无效: 无效的JSON大小 %1").arg(jsonSize));
        qDebug() << "文件头格式错误: 无效的JSON大小" << jsonSize;
        
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        
        return;
    }
    
    // 提取JSON数据
    QByteArray jsonData = headerData.mid(sizeof(qint32), jsonSize);
    
    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        emit error(QString("文件头信息格式无效: %1").arg(parseError.errorString()));
        qDebug() << "文件头JSON解析失败, 错误:" << parseError.errorString() 
                 << ", 大小:" << jsonSize 
                 << ", 数据:" << jsonData.left(50) << "...";
        
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        
        return;
    }
    
    QJsonObject fileInfo = jsonDoc.object();
    
    // 检查是否有错误信息
    if (fileInfo.contains("error")) {
        emit error(fileInfo["error"].toString());
        
        if (m_file) {
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        
        return;
    }
    
    // 解析文件信息
    m_fileSize = fileInfo["fileSize"].toString().toLongLong();
    m_receivingHeader = false;
    
    qDebug() << "开始接收文件:" << m_currentFileName << ", 大小:" << m_fileSize << "字节";
}

void TcpClient::processFileList(const QByteArray &listData)
{
    QJsonDocument jsonDoc = QJsonDocument::fromJson(listData);
    
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        // 可能数据还不完整，继续等待更多数据
        return;
    }
    
    QJsonObject listObj = jsonDoc.object();
    
    // 如果含有错误信息
    if (listObj.contains("error")) {
        emit error(listObj["error"].toString());
        m_receivingFileList = false;
        m_fileListBuffer.clear();
        return;
    }
    
    // 如果含有文件列表
    if (listObj.contains("files") && listObj.contains("count")) {
        QList<QPair<QString, qint64>> fileList;
        QJsonArray filesArray = listObj["files"].toArray();
        
        for (int i = 0; i < filesArray.size(); ++i) {
            QJsonObject fileObj = filesArray[i].toObject();
            QString fileName = fileObj["name"].toString();
            qint64 fileSize = fileObj["size"].toString().toLongLong();
            fileList.append(qMakePair(fileName, fileSize));
        }
        
        emit fileListReceived(fileList);
        m_receivingFileList = false;
        m_fileListBuffer.clear();
    }
}

void TcpClient::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    // 命令发送完毕，等待服务器响应
}

void TcpClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;
    
    // 针对不同类型的错误提供更友好的消息
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMessage = "连接被拒绝，请确认服务器是否正在运行";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMessage = "找不到主机，请检查服务器地址是否正确";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMessage = "连接超时，请检查网络连接";
        break;
    case QAbstractSocket::NetworkError:
        errorMessage = "网络错误，请检查网络连接";
        break;
    default:
        errorMessage = QString("Socket错误: %1").arg(m_socket->errorString());
        break;
    }
    
    qDebug() << "Socket错误类型:" << socketError << ", 详细信息:" << m_socket->errorString();
    qDebug() << "友好消息:" << errorMessage;
    
    emit error(errorMessage);
    
    // 清理资源
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void TcpClient::finishFileDownload()
{
    if (!m_file) {
        return;
    }
    
    QString fileName = m_currentFileName;
    qint64 fileSize = m_fileSize;
    
    m_file->close();
    
    // 发送文件接收完成信号
    emit fileReceived(fileName, fileSize);
    qDebug() << "文件接收完成:" << fileName << ", 大小:" << fileSize << "字节";
    
    // 清理资源
    delete m_file;
    m_file = nullptr;
    m_currentFileName.clear();
    m_fileSize = 0;
    m_bytesReceived = 0;
}

#ifdef Q_OS_ANDROID
bool TcpClient::requestStoragePermissions()
{
    // 简化实现，只记录日志信息
    qDebug() << "Android存储权限请求 - 需要用户在系统设置中手动授予权限";
    qDebug() << "如果遇到存储问题，请前往系统设置 -> 应用 -> TCP文件传输 -> 权限，并启用存储权限";
    return false;
}
#endif 