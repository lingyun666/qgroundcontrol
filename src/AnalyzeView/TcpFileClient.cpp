#include "TcpFileClient.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>

// 命令常量定义 - 必须与服务器端定义一致
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
    m_socket->flush();
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
    
    // 发送下载请求命令 - 格式必须为: "GET 文件名"，与服务器parseCommand匹配
    QByteArray command = CMD_GET + " " + fileName.toUtf8();
    qDebug() << "发送命令:" << command;
    m_socket->write(command);
    m_socket->flush();
    
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
        if (m_socket->bytesAvailable() <= 0) {
            qDebug() << "文件列表数据接收完成，数据大小:" << m_fileListBuffer.size() << "字节";
            processFileList(m_fileListBuffer);
            m_fileListBuffer.clear();
            m_receivingFileList = false;
        }
        return;
    }
    
    if (m_receivingHeader) {
        // 接收文件头信息，服务器使用固定大小的头部
        static const int HEADER_SIZE = 1024; // 从服务器代码看，头部大小是1024字节
        
        m_headerBuffer.append(m_socket->readAll());
        
        if (m_headerBuffer.size() >= HEADER_SIZE) {
            // 读取完整的头部
            processFileHeader(m_headerBuffer.left(HEADER_SIZE));
            
            // 保存剩余的数据
            QByteArray remainingData = m_headerBuffer.mid(HEADER_SIZE);
            m_headerBuffer.clear();
            m_receivingHeader = false;
            
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
        QByteArray data = m_socket->readAll();
        qDebug() << "收到未预期的数据:" << data.size() << "字节";
        
        // 尝试解析为JSON，可能是错误消息
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error")) {
                QString errorMsg = obj["error"].toString();
                qDebug() << "服务器返回错误:" << errorMsg;
                emit error(errorMsg);
            }
        }
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
    qDebug() << "处理文件头信息，大小:" << headerData.size() << "字节";
    
    // 根据服务器的实现，解析头部数据
    // 头部的前4个字节是JSON数据的长度
    QDataStream stream(headerData);
    stream.setVersion(QDataStream::Qt_6_0);
    
    qint32 jsonSize = 0;
    stream >> jsonSize;
    
    if (jsonSize <= 0 || jsonSize > headerData.size() - 4) {
        qDebug() << "无效的JSON大小:" << jsonSize;
        return;
    }
    
    QByteArray jsonData = headerData.mid(4, jsonSize);
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "解析JSON头部失败:" << parseError.errorString();
        return;
    }
    
    QJsonObject headerObj = doc.object();
    qDebug() << "文件头JSON:" << headerObj;
    
    // 获取文件大小
    if (headerObj.contains("fileSize")) {
        QString fileSizeStr = headerObj["fileSize"].toString();
        bool ok;
        m_fileSize = fileSizeStr.toLongLong(&ok);
        if (ok) {
            qDebug() << "文件大小:" << m_fileSize;
        } else {
            qDebug() << "无法解析文件大小:" << fileSizeStr;
            m_fileSize = 0;
        }
    } else {
        qDebug() << "警告: 头部中未找到文件大小信息";
        m_fileSize = 0;
    }
}

void TcpFileClient::processFileList(const QByteArray &listData)
{
    qDebug() << "处理文件列表数据, 大小:" << listData.size() << "字节";
    
    // 调试输出原始数据的前100个字节
    QString debugData = QString::fromUtf8(listData.left(100));
    qDebug() << "原始数据前100字节:" << debugData << (listData.size() > 100 ? "..." : "");
    
    QList<QPair<QString, qint64>> fileList;
    
    // 解析为JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(listData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析失败:" << parseError.errorString() << ", 错误位置:" << parseError.offset;
        emit error("解析文件列表失败");
        
        // 尝试简单文本格式解析
        QString text = QString::fromUtf8(listData);
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        qDebug() << "尝试文本格式解析，行数:" << lines.size();
        
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            
            // 可能的格式: 文件名|大小
            int separatorPos = trimmed.lastIndexOf('|');
            if (separatorPos > 0) {
                QString name = trimmed.left(separatorPos).trimmed();
                bool ok;
                qint64 size = trimmed.mid(separatorPos + 1).trimmed().toLongLong(&ok);
                if (!name.isEmpty()) {
                    qDebug() << "添加文件:" << name << "大小:" << size;
                    fileList.append(qMakePair(name, ok ? size : 0));
                }
            } else {
                // 只有文件名
                qDebug() << "添加文件(仅名称):" << trimmed;
                fileList.append(qMakePair(trimmed, qint64(0)));
            }
        }
        
        if (!fileList.isEmpty()) {
            qDebug() << "通过文本格式解析成功, 文件数:" << fileList.size();
            emit fileListReceived(fileList);
            return;
        }
    }
    
    if (doc.isObject()) {
        QJsonObject rootObj = doc.object();
        qDebug() << "JSON是对象，键:" << rootObj.keys().join(", ");
        
        // 服务器格式: {"files": [...], "count": n}
        if (rootObj.contains("files") && rootObj["files"].isArray()) {
            QJsonArray filesArray = rootObj["files"].toArray();
            qDebug() << "files数组包含" << filesArray.size() << "个元素";
            
            for (int i = 0; i < filesArray.size(); ++i) {
                QJsonValue value = filesArray[i];
                if (value.isObject()) {
                    QJsonObject fileObj = value.toObject();
                    QString name;
                    qint64 size = 0;
                    
                    if (fileObj.contains("name")) {
                        name = fileObj["name"].toString();
                    } else {
                        qDebug() << "文件对象缺少name字段:" << QJsonDocument(fileObj).toJson();
                        continue;
                    }
                    
                    if (fileObj.contains("size")) {
                        QJsonValue sizeValue = fileObj["size"];
                        if (sizeValue.isString()) {
                            bool ok;
                            size = sizeValue.toString().toLongLong(&ok);
                            if (!ok) size = 0;
                        } else if (sizeValue.isDouble()) {
                            size = static_cast<qint64>(sizeValue.toDouble());
                        }
                    }
                    
                    qDebug() << "添加文件:" << name << "大小:" << size;
                    fileList.append(qMakePair(name, size));
                } else if (value.isString()) {
                    // 简单字符串列表
                    QString name = value.toString();
                    qDebug() << "添加文件(字符串):" << name;
                    fileList.append(qMakePair(name, qint64(0)));
                }
            }
        } else {
            // 尝试解析其他格式
            for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
                if (it.key() != "count") {  // 跳过计数字段
                    QString name = it.key();
                    qint64 size = 0;
                    
                    if (it.value().isObject()) {
                        QJsonObject fileObj = it.value().toObject();
                        if (fileObj.contains("size")) {
                            QJsonValue sizeValue = fileObj["size"];
                            if (sizeValue.isString()) {
                                bool ok;
                                size = sizeValue.toString().toLongLong(&ok);
                                if (!ok) size = 0;
                            } else if (sizeValue.isDouble()) {
                                size = static_cast<qint64>(sizeValue.toDouble());
                            }
                        }
                    } else if (it.value().isDouble()) {
                        size = static_cast<qint64>(it.value().toDouble());
                    } else if (it.value().isString()) {
                        bool ok;
                        size = it.value().toString().toLongLong(&ok);
                        if (!ok) size = 0;
                    }
                    
                    qDebug() << "添加文件(从键值对):" << name << "大小:" << size;
                    fileList.append(qMakePair(name, size));
                }
            }
        }
    } else if (doc.isArray()) {
        // 文件数组格式
        QJsonArray array = doc.array();
        qDebug() << "JSON是数组，包含" << array.size() << "个元素";
        
        for (int i = 0; i < array.size(); ++i) {
            QJsonValue value = array[i];
            if (value.isObject()) {
                QJsonObject fileObj = value.toObject();
                QString name;
                qint64 size = 0;
                
                if (fileObj.contains("name")) {
                    name = fileObj["name"].toString();
                } else {
                    QStringList keys = fileObj.keys();
                    if (!keys.isEmpty()) {
                        name = keys.first(); // 使用第一个键作为文件名
                    }
                }
                
                if (fileObj.contains("size")) {
                    QJsonValue sizeValue = fileObj["size"];
                    if (sizeValue.isString()) {
                        bool ok;
                        size = sizeValue.toString().toLongLong(&ok);
                        if (!ok) size = 0;
                    } else if (sizeValue.isDouble()) {
                        size = static_cast<qint64>(sizeValue.toDouble());
                    }
                }
                
                if (!name.isEmpty()) {
                    qDebug() << "添加文件(从数组对象):" << name << "大小:" << size;
                    fileList.append(qMakePair(name, size));
                }
            } else if (value.isString()) {
                // 字符串数组
                QString name = value.toString();
                qDebug() << "添加文件(从数组字符串):" << name;
                fileList.append(qMakePair(name, qint64(0)));
            }
        }
    }
    
    // 如果通过JSON解析没找到文件，尝试解析为文本
    if (fileList.isEmpty()) {
        qDebug() << "JSON解析未找到文件，尝试文本格式";
        QString text = QString::fromUtf8(listData);
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
            
            // 可能的格式: 文件名|大小
            int separatorPos = trimmed.lastIndexOf('|');
            if (separatorPos > 0) {
                QString name = trimmed.left(separatorPos).trimmed();
                bool ok;
                qint64 size = trimmed.mid(separatorPos + 1).trimmed().toLongLong(&ok);
                if (!name.isEmpty()) {
                    qDebug() << "添加文件(文本格式):" << name << "大小:" << size;
                    fileList.append(qMakePair(name, ok ? size : 0));
                }
            } else {
                // 只有文件名
                qDebug() << "添加文件(仅名称):" << trimmed;
                fileList.append(qMakePair(trimmed, qint64(0)));
            }
        }
    }
    
    qDebug() << "最终解析到" << fileList.size() << "个文件";
    
    // 即使没有找到文件也发送信号，让UI能够显示"没有文件"的提示
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
