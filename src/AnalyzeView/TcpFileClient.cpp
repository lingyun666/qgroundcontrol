#include "TcpFileClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>

// 命令常量定义
const QByteArray TcpFileClient::CMD_LIST = "LIST";
const QByteArray TcpFileClient::CMD_GET = "GET ";

TcpFileClient::TcpFileClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_file(nullptr)
    , m_fileSize(0)
    , m_bytesReceived(0)
    , m_receivingHeader(false)
    , m_receivingFileList(false)
{
    // 连接socket信号
    connect(m_socket, &QTcpSocket::connected, this, &TcpFileClient::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &TcpFileClient::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpFileClient::onReadyRead);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpFileClient::onBytesWritten);
    connect(m_socket, static_cast<void (QTcpSocket::*)(QAbstractSocket::SocketError)>(&QTcpSocket::errorOccurred), 
            this, &TcpFileClient::onError);
    
    qDebug() << "TCP文件客户端初始化完成";
}

TcpFileClient::~TcpFileClient()
{
    disconnectFromServer();
    
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    
    delete m_socket;
}

bool TcpFileClient::connectToServer(const QString &host, quint16 port)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "已经连接到服务器";
        return true;
    }
    
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "正在连接或关闭中，请稍后重试";
        return false;
    }
    
    qDebug() << "连接到服务器:" << host << ":" << port;
    m_socket->connectToHost(host, port);
    
    // 等待连接完成
    if (!m_socket->waitForConnected(5000)) {
        emit error("连接服务器超时");
        return false;
    }
    
    return true;
}

void TcpFileClient::disconnectFromServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "断开与服务器的连接";
        m_socket->disconnectFromHost();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->waitForDisconnected(1000);
        }
    }
}

void TcpFileClient::requestFileList()
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return;
    }
    
    qDebug() << "请求文件列表";
    m_receivingFileList = true;
    m_fileListBuffer.clear();
    
    // 发送列表请求命令
    m_socket->write(CMD_LIST);
}

bool TcpFileClient::downloadFile(const QString &fileName, const QString &savePath)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit error("未连接到服务器");
        return false;
    }
    
    if (m_file) {
        emit error("已有下载任务在进行中");
        return false;
    }
    
    // 准备保存文件
    QDir dir(savePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit error(QString("无法创建目录: %1").arg(savePath));
            return false;
        }
    }
    
    QString filePath = savePath + "/" + fileName;
    m_file = new QFile(filePath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit error(QString("无法打开文件进行写入: %1").arg(filePath));
        delete m_file;
        m_file = nullptr;
        return false;
    }
    
    qDebug() << "开始下载文件:" << fileName << "保存到:" << filePath;
    m_currentFileName = fileName;
    m_fileSize = 0;
    m_bytesReceived = 0;
    m_receivingHeader = true;
    m_headerBuffer.clear();
    
    // 发送下载请求命令
    QByteArray command = CMD_GET + fileName.toUtf8();
    m_socket->write(command);
    
    return true;
}

bool TcpFileClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpFileClient::onConnected()
{
    qDebug() << "已连接到服务器";
    emit connectionStateChanged(true);
}

void TcpFileClient::onDisconnected()
{
    qDebug() << "已断开与服务器的连接";
    
    // 如果有下载任务，关闭文件
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    
    emit connectionStateChanged(false);
}

void TcpFileClient::onReadyRead()
{
    if (m_receivingFileList) {
        // 处理文件列表响应
        m_fileListBuffer.append(m_socket->readAll());
        
        // 检查是否接收到完整的数据
        // 注意：这里不再自动调用processFileList并重置m_receivingFileList标志
        // 而是保留接收状态，直到接收完全部数据
        if (m_socket->bytesAvailable() <= 0) {
            processFileList(m_fileListBuffer);
            m_receivingFileList = false;
        }
        return;
    }
    
    if (m_receivingHeader) {
        // 接收文件头信息，格式为JSON
        m_headerBuffer.append(m_socket->readAll());
        
        // 查找头部结束标志
        int endPos = m_headerBuffer.indexOf("\r\n\r\n");
        if (endPos != -1) {
            // 处理头部信息
            QByteArray header = m_headerBuffer.left(endPos);
            processFileHeader(header);
            
            // 保存剩余的数据
            QByteArray remainingData = m_headerBuffer.mid(endPos + 4); // 4是\r\n\r\n的长度
            m_receivingHeader = false;
            m_headerBuffer.clear();
            
            // 如果有剩余数据，处理文件内容
            if (!remainingData.isEmpty()) {
                m_file->write(remainingData);
                m_bytesReceived += remainingData.size();
                
                // 更新进度
                if (m_fileSize > 0) {
                    int progress = static_cast<int>((m_bytesReceived * 100) / m_fileSize);
                    emit downloadProgress(m_currentFileName, progress);
                }
                
                // 检查是否下载完成
                if (m_bytesReceived >= m_fileSize) {
                    finishFileDownload();
                }
            }
        }
    } else if (m_file) {
        // 接收文件内容
        QByteArray data = m_socket->readAll();
        m_file->write(data);
        m_bytesReceived += data.size();
        
        // 更新进度
        if (m_fileSize > 0) {
            int progress = static_cast<int>((m_bytesReceived * 100) / m_fileSize);
            emit downloadProgress(m_currentFileName, progress);
        }
        
        // 检查是否下载完成
        if (m_bytesReceived >= m_fileSize) {
            finishFileDownload();
        }
    } else {
        // 未处于任何接收状态，但收到了数据，可能是服务器主动发送的消息
        // 读取并丢弃数据，避免缓冲区堆积
        m_socket->readAll();
        qDebug() << "收到未预期的数据，已丢弃";
    }
}

void TcpFileClient::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);
    // 可以用于跟踪命令发送状态，但目前不需要特殊处理
}

void TcpFileClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMsg;
    
    switch (socketError) {
        case QAbstractSocket::ConnectionRefusedError:
            errorMsg = "连接被拒绝";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorMsg = "远程主机关闭了连接";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorMsg = "无法找到主机";
            break;
        case QAbstractSocket::SocketTimeoutError:
            errorMsg = "连接超时";
            break;
        case QAbstractSocket::NetworkError:
            errorMsg = "网络错误";
            break;
        default:
            errorMsg = "网络错误: " + m_socket->errorString();
            break;
    }
    
    qDebug() << "Socket错误:" << errorMsg;
    emit error(errorMsg);
    
    // 如果有下载任务，关闭文件
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void TcpFileClient::processFileHeader(const QByteArray &headerData)
{
    qDebug() << "处理文件头信息:" << headerData;
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(headerData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "解析JSON头部失败:" << parseError.errorString();
        
        // 尝试直接解析简单的头部格式
        QString headerStr = QString::fromUtf8(headerData);
        QStringList lines = headerStr.split("\r\n");
        
        for (const QString &line : lines) {
            if (line.startsWith("Content-Length:")) {
                bool ok;
                m_fileSize = line.mid(15).trimmed().toLongLong(&ok);
                if (ok) {
                    qDebug() << "文件大小:" << m_fileSize;
                }
                break;
            }
        }
        
        return;
    }
    
    QJsonObject headerObj = doc.object();
    
    // 获取文件大小
    if (headerObj.contains("size") || headerObj.contains("fileSize") || headerObj.contains("contentLength")) {
        if (headerObj.contains("size")) {
            m_fileSize = headerObj["size"].toVariant().toLongLong();
        } else if (headerObj.contains("fileSize")) {
            m_fileSize = headerObj["fileSize"].toVariant().toLongLong();
        } else {
            m_fileSize = headerObj["contentLength"].toVariant().toLongLong();
        }
        qDebug() << "文件大小:" << m_fileSize;
    } else {
        qDebug() << "警告: 头部中未找到文件大小信息";
        m_fileSize = 0;
    }
}

void TcpFileClient::processFileList(const QByteArray &listData)
{
    qDebug() << "处理文件列表数据:" << listData;
    
    QList<QPair<QString, qint64>> fileList;
    
    // 首先尝试解析为JSON格式
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(listData, &parseError);
    
    if (parseError.error == QJsonParseError::NoError) {
        qDebug() << "成功解析为JSON格式";
        if (doc.isArray()) {
            QJsonArray filesArray = doc.array();
            qDebug() << "JSON是数组，包含" << filesArray.size() << "个元素";
            
            for (const QJsonValue &value : filesArray) {
                if (value.isObject()) {
                    QJsonObject fileObj = value.toObject();
                    QString name;
                    qint64 size = 0;
                    
                    if (fileObj.contains("name")) {
                        name = fileObj["name"].toString();
                    } else if (fileObj.contains("fileName")) {
                        name = fileObj["fileName"].toString();
                    }
                    
                    if (fileObj.contains("size")) {
                        size = fileObj["size"].toVariant().toLongLong();
                    } else if (fileObj.contains("fileSize")) {
                        size = fileObj["fileSize"].toVariant().toLongLong();
                    }
                    
                    if (!name.isEmpty()) {
                        qDebug() << "添加文件:" << name << "大小:" << size;
                        fileList.append(qMakePair(name, size));
                    }
                }
            }
        } else if (doc.isObject()) {
            // 可能是包含files数组的对象
            QJsonObject rootObj = doc.object();
            qDebug() << "JSON是对象，键:" << rootObj.keys();
            
            if (rootObj.contains("files") && rootObj["files"].isArray()) {
                QJsonArray filesArray = rootObj["files"].toArray();
                qDebug() << "files数组包含" << filesArray.size() << "个元素";
                
                for (const QJsonValue &value : filesArray) {
                    if (value.isObject()) {
                        QJsonObject fileObj = value.toObject();
                        QString name;
                        qint64 size = 0;
                        
                        if (fileObj.contains("name")) {
                            name = fileObj["name"].toString();
                        } else if (fileObj.contains("fileName")) {
                            name = fileObj["fileName"].toString();
                        }
                        
                        if (fileObj.contains("size")) {
                            size = fileObj["size"].toVariant().toLongLong();
                        } else if (fileObj.contains("fileSize")) {
                            size = fileObj["fileSize"].toVariant().toLongLong();
                        }
                        
                        if (!name.isEmpty()) {
                            qDebug() << "添加文件:" << name << "大小:" << size;
                            fileList.append(qMakePair(name, size));
                        }
                    } else if (value.isString()) {
                        // 简单字符串列表
                        QString name = value.toString();
                        qDebug() << "添加文件(字符串):" << name;
                        fileList.append(qMakePair(name, qint64(0)));
                    }
                }
            }
        }
    } else {
        qDebug() << "JSON解析失败:" << parseError.errorString() << "，尝试解析为文本格式";
        // 尝试解析为简单的文本格式，每行一个文件
        QString listStr = QString::fromUtf8(listData);
        QStringList lines = listStr.split("\n", Qt::SkipEmptyParts);
        
        qDebug() << "文本格式，包含" << lines.size() << "行";
        
        for (const QString &line : lines) {
            QString trimmedLine = line.trimmed();
            if (trimmedLine.isEmpty()) {
                continue;
            }
            
            // 尝试解析格式: 文件名|大小
            int separatorPos = trimmedLine.lastIndexOf('|');
            if (separatorPos > 0) {
                QString name = trimmedLine.left(separatorPos).trimmed();
                bool ok;
                qint64 size = trimmedLine.mid(separatorPos + 1).trimmed().toLongLong(&ok);
                
                if (!name.isEmpty()) {
                    qDebug() << "添加文件(带分隔符):" << name << "大小:" << size;
                    fileList.append(qMakePair(name, ok ? size : 0));
                }
            } else {
                // 仅有文件名
                qDebug() << "添加文件(仅名称):" << trimmedLine;
                fileList.append(qMakePair(trimmedLine, qint64(0)));
            }
        }
    }
    
    qDebug() << "解析到" << fileList.size() << "个文件";
    
    // 输出原始数据的十六进制和ASCII表示，用于调试
    QString hexDump;
    QString asciiDump;
    for (int i = 0; i < qMin(listData.size(), 200); ++i) {
        unsigned char c = static_cast<unsigned char>(listData[i]);
        hexDump += QString("%1 ").arg(c, 2, 16, QChar('0'));
        asciiDump += (c >= 32 && c <= 126) ? QChar(c) : '.';
    }
    qDebug() << "原始数据前200字节(Hex):" << hexDump;
    qDebug() << "原始数据前200字节(ASCII):" << asciiDump;
    
    emit fileListReceived(fileList);
}

void TcpFileClient::finishFileDownload()
{
    if (!m_file) {
        return;
    }
    
    m_file->flush();
    m_file->close();
    
    qint64 actualSize = m_file->size();
    
    if (m_fileSize > 0 && actualSize != m_fileSize) {
        qDebug() << "警告: 文件大小不匹配, 预期:" << m_fileSize << "实际:" << actualSize;
    }
    
    QString fileName = m_currentFileName;
    qint64 size = actualSize;
    
    delete m_file;
    m_file = nullptr;
    m_currentFileName.clear();
    
    qDebug() << "文件下载完成:" << fileName << "大小:" << size;
    emit fileReceived(fileName, size);
} 
