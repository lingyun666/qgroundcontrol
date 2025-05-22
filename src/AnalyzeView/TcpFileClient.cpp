#include "TcpFileClient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>

// 定义命令常量
const QByteArray TcpFileClient::CMD_LIST = "LIST";
const QByteArray TcpFileClient::CMD_GET = "GET";

TcpFileClient::TcpFileClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_file(nullptr)
    , m_fileSize(0)
    , m_bytesReceived(0)
    , m_receivingHeader(false)
    , m_receivingFileList(false)
{
    // 连接信号和槽
    connect(m_socket, &QTcpSocket::connected, this, &TcpFileClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpFileClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpFileClient::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpFileClient::onBytesWritten);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), 
            this, &TcpFileClient::onError);
}

TcpFileClient::~TcpFileClient()
{
    disconnectFromServer();
}

bool TcpFileClient::connectToServer(const QString &host, quint16 port)
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

void TcpFileClient::disconnectFromServer()
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

void TcpFileClient::requestFileList()
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

bool TcpFileClient::downloadFile(const QString &fileName, const QString &savePath)
{
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
    QString fullPath = savePath;
    if (!savePath.endsWith('/')) {
        fullPath += '/';
    }
    fullPath += fileName;
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

bool TcpFileClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpFileClient::onConnected()
{
    qDebug() << "已连接到服务器:" << m_socket->peerAddress().toString() << ":" << m_socket->peerPort();
    emit connectionStateChanged(true);
}

void TcpFileClient::onDisconnected()
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

void TcpFileClient::onReadyRead()
{
    while (m_socket->bytesAvailable() > 0) {
        // 处理文件列表
        if (m_receivingFileList) {
            QByteArray data = m_socket->readAll();
            m_fileListBuffer.append(data);
            
            // 检查是否接收到完整的文件列表
            if (m_fileListBuffer.contains("\r\n\r\n")) {
                processFileList(m_fileListBuffer);
                m_fileListBuffer.clear();
                m_receivingFileList = false;
            }
            continue;
        }
        
        // 处理文件头信息
        if (m_receivingHeader) {
            QByteArray data = m_socket->read(HEADER_SIZE - m_headerBuffer.size());
            m_headerBuffer.append(data);
            
            if (m_headerBuffer.size() >= HEADER_SIZE) {
                processFileHeader(m_headerBuffer);
                m_headerBuffer.clear();
                m_receivingHeader = false;
            }
            continue;
        }
        
        // 处理文件数据
        if (m_file) {
            // 计算要读取的字节数
            qint64 bytesToRead = qMin(m_socket->bytesAvailable(), m_fileSize - m_bytesReceived);
            
            if (bytesToRead > 0) {
                QByteArray data = m_socket->read(bytesToRead);
                m_bytesReceived += data.size();
                
                // 写入文件
                if (m_file->write(data) != data.size()) {
                    emit error(QString("写入文件失败: %1").arg(m_file->errorString()));
                    finishFileDownload();
                    return;
                }
                
                // 更新进度
                int progress = static_cast<int>((static_cast<double>(m_bytesReceived) / m_fileSize) * 100);
                emit downloadProgress(m_currentFileName, progress);
                
                // 检查是否下载完成
                if (m_bytesReceived >= m_fileSize) {
                    finishFileDownload();
                }
            }
        }
    }
}

void TcpFileClient::processFileHeader(const QByteArray &headerData)
{
    QDataStream stream(headerData);
    stream.setVersion(QDataStream::Qt_5_15);
    
    // 读取文件名和大小
    QString fileName;
    qint64 fileSize;
    
    stream >> fileName >> fileSize;
    
    qDebug() << "收到文件头信息: 文件名=" << fileName << ", 大小=" << fileSize << "字节";
    
    // 检查文件名匹配
    if (fileName != m_currentFileName) {
        emit error(QString("收到错误的文件: %1 (期望: %2)").arg(fileName, m_currentFileName));
        finishFileDownload();
        return;
    }
    
    m_fileSize = fileSize;
    m_bytesReceived = 0;
}

void TcpFileClient::processFileList(const QByteArray &listData)
{
    QList<QPair<QString, qint64>> fileList;
    
    // 解析文件列表数据
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(listData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        emit error(QString("解析文件列表失败: %1").arg(parseError.errorString()));
        return;
    }
    
    QJsonObject rootObj = doc.object();
    QJsonArray filesArray = rootObj["files"].toArray();
    
    // 构建文件列表
    for (int i = 0; i < filesArray.size(); i++) {
        QJsonObject fileObj = filesArray[i].toObject();
        QString fileName = fileObj["name"].toString();
        qint64 fileSize = fileObj["size"].toVariant().toLongLong();
        
        fileList.append(qMakePair(fileName, fileSize));
    }
    
    qDebug() << "收到文件列表, 包含" << fileList.size() << "个文件";
    emit fileListReceived(fileList);
}

void TcpFileClient::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
}

void TcpFileClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMessage = "连接被拒绝";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        errorMessage = "远程主机关闭了连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMessage = "未找到主机";
        break;
    case QAbstractSocket::SocketAccessError:
        errorMessage = "套接字访问错误";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMessage = "连接超时";
        break;
    case QAbstractSocket::NetworkError:
        errorMessage = "网络错误";
        break;
    default:
        errorMessage = QString("发生网络错误: %1").arg(m_socket->errorString());
        break;
    }
    
    qDebug() << "TCP错误: " << errorMessage;
    emit error(errorMessage);
}

void TcpFileClient::finishFileDownload()
{
    if (!m_file) {
        return;
    }
    
    QString fileName = m_currentFileName;
    qint64 fileSize = m_fileSize;
    
    // 关闭文件并清理资源
    m_file->close();
    delete m_file;
    m_file = nullptr;
    m_currentFileName.clear();
    m_fileSize = 0;
    m_bytesReceived = 0;
    
    qDebug() << "文件下载完成: " << fileName << ", 大小: " << fileSize << "字节";
    emit fileReceived(fileName, fileSize);
} 