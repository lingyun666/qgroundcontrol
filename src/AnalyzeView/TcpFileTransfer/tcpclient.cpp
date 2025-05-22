#include "tcpclient.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QDebug>
#include <QDir>
#include <QDataStream>
#include <algorithm>

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
    if (!isConnected()) {
        emit error("未连接到服务器，无法请求文件列表");
        return;
    }
    
    // 发送请求文件列表命令
    QByteArray command = CMD_LIST;
    if (m_socket->write(command) != command.size()) {
        QString errorMsg = QString("发送获取文件列表命令失败: %1").arg(m_socket->errorString());
        qDebug() << errorMsg;
        emit error(errorMsg);
        return;
    }
    
    // 标记开始接收文件列表
    m_receivingFileList = true;
    m_fileListBuffer.clear();
    
    qDebug() << "已发送文件列表请求命令";
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
    qDebug() << "接收到数据，可读字节数:" << m_socket->bytesAvailable() 
             << "接收文件头:" << m_receivingHeader 
             << "接收文件列表:" << m_receivingFileList;
    
    // 处理接收到的数据
    if (m_receivingHeader) {
        // 接收文件头
        while (m_socket->bytesAvailable() > 0 && m_headerBuffer.size() < HEADER_SIZE) {
            m_headerBuffer.append(m_socket->read(HEADER_SIZE - m_headerBuffer.size()));
        }
        
        // 当收到完整的头部数据后进行处理
        if (m_headerBuffer.size() >= HEADER_SIZE) {
            processFileHeader(m_headerBuffer);
            m_receivingHeader = false;
            m_headerBuffer.clear();
        }
    } else if (m_receivingFileList) {
        // 接收文件列表数据
        QByteArray data = m_socket->readAll();
        m_fileListBuffer.append(data);
        
        qDebug() << "接收到文件列表数据块，大小:" << data.size() 
                 << "，总缓冲区大小:" << m_fileListBuffer.size();
        
        // 输出收到的数据前100个字节以便调试
        if (!data.isEmpty()) {
            QString hexString;
            qsizetype maxChars = qMin(data.size(), qsizetype(100));
            for (qsizetype i = 0; i < maxChars; ++i) {
                hexString += QString("%1 ").arg((unsigned char)data.at(i), 2, 16, QChar('0'));
            }
            qDebug() << "接收到的数据(十六进制):" << hexString;
            
            // 显示ASCII文本 (如果可能)
            QString textData = QString::fromUtf8(data.left(maxChars));
            qDebug() << "接收到的数据(文本):" << textData;
        }
        
        // 检查数据是否看起来像完整的JSON，或者如果数据超过6KB就尝试处理
        if (m_fileListBuffer.startsWith('[') && (m_fileListBuffer.endsWith(']') || m_fileListBuffer.contains('\0'))) {
            qDebug() << "数据看起来像完整的JSON，尝试处理";
            processFileList(m_fileListBuffer);
            m_receivingFileList = false;
            m_fileListBuffer.clear();
        } 
        // 如果接收了足够多的数据，也尝试处理
        else if (m_fileListBuffer.size() > 6000 || !m_socket->waitForReadyRead(100)) {
            qDebug() << "接收数据似乎已完成，尝试处理累积的数据 (大小:" << m_fileListBuffer.size() << "字节)";
            processFileList(m_fileListBuffer);
            m_receivingFileList = false;
            m_fileListBuffer.clear();
        }
    } else if (m_file) {
        // 接收文件数据
        QByteArray data = m_socket->readAll();
        qint64 bytesWritten = m_file->write(data);
        
        if (bytesWritten != data.size()) {
            emit error(QString("写入文件失败: %1").arg(m_file->errorString()));
            finishFileDownload();
            return;
        }
        
        m_bytesReceived += bytesWritten;
        
        // 计算下载进度
        int progress = static_cast<int>((static_cast<double>(m_bytesReceived) / m_fileSize) * 100);
        emit downloadProgress(m_currentFileName, progress);
        
        // 检查是否下载完成
        if (m_bytesReceived >= m_fileSize) {
            finishFileDownload();
        }
    } else {
        // 未处理的数据，可能是未预期的响应
        QByteArray data = m_socket->readAll();
        qDebug() << "接收到未处理的数据，大小:" << data.size();
        
        // 输出收到的数据前100个字节以便调试
        if (!data.isEmpty()) {
            QString hexString;
            qsizetype maxChars = qMin(data.size(), qsizetype(100));
            for (qsizetype i = 0; i < maxChars; ++i) {
                hexString += QString("%1 ").arg((unsigned char)data.at(i), 2, 16, QChar('0'));
            }
            qDebug() << "未处理的数据(十六进制):" << hexString;
            
            // 显示ASCII文本 (如果可能)
            QString textData = QString::fromUtf8(data.left(maxChars));
            qDebug() << "未处理的数据(文本):" << textData;
        }
        
        // 检查数据是否可能是文件列表
        if (data.contains('[') || data.contains("name")) {
            qDebug() << "未处理的数据可能是文件列表，尝试处理";
            processFileList(data);
        }
    }
}

void TcpClient::processFileHeader(const QByteArray &headerData)
{
    QDataStream stream(headerData);
    stream.setByteOrder(QDataStream::BigEndian);
    
    // 读取文件大小和名称
    stream >> m_fileSize;
    
    QByteArray fileNameBytes;
    stream >> fileNameBytes;
    QString fileName = QString::fromUtf8(fileNameBytes);
    
    // 如果文件名与请求的不一致，使用发送的文件名
    if (fileName != m_currentFileName) {
        qDebug() << "收到的文件名与请求的不一致: 请求=" << m_currentFileName << "收到=" << fileName;
        m_currentFileName = fileName;
    }
    
    qDebug() << "收到文件头信息: 文件名=" << m_currentFileName << "大小=" << m_fileSize << "字节";
    
    // 如果文件大小为0，可能表示文件不存在
    if (m_fileSize <= 0) {
        emit error(QString("请求的文件 %1 不存在或大小为0").arg(m_currentFileName));
        finishFileDownload();
    }
}

void TcpClient::processFileList(const QByteArray &listData)
{
    QList<QPair<QString, qint64>> fileList;
    QByteArray jsonData = listData;
    
    qDebug() << "处理文件列表数据，大小:" << jsonData.size() 
             << "，数据前50字节:" << jsonData.left(50);
    
    // 检查返回的数据中可能包含的HTML标签等干扰信息
    int startIndex = jsonData.indexOf('[');
    if (startIndex < 0) {
        // 尝试搜索{"files":
        startIndex = jsonData.indexOf("{\"files\":");
        if (startIndex >= 0) {
            startIndex = jsonData.indexOf('[', startIndex);
        }
    }
    
    if (startIndex < 0) {
        // 最后一个尝试：查找任何可能的JSON格式
        for (int i = 0; i < jsonData.size(); ++i) {
            char c = jsonData.at(i);
            if (c == '{' || c == '[') {
                startIndex = i;
                break;
            }
        }
        
        if (startIndex < 0) {
            qDebug() << "无法在数据中找到JSON开始标记";
            emit error("无法在接收数据中找到JSON格式");
            
            // 记录全部数据进行调试
            qDebug() << "收到的完整数据:" << jsonData;
            
            // 尝试构建一个空的文件列表
            fileList.clear();
            emit fileListReceived(fileList);
            return;
        }
    }
    
    // 查找JSON结束位置
    int endIndex = -1;
    if (jsonData.indexOf('[', startIndex) == startIndex) {
        // 寻找匹配的 ]
        int openBrackets = 1;
        for (int i = startIndex + 1; i < jsonData.size(); i++) {
            char c = jsonData[i];
            if (c == '[') openBrackets++;
            else if (c == ']') {
                openBrackets--;
                if (openBrackets == 0) {
                    endIndex = i;
                    break;
                }
            }
        }
        
        // 如果找不到结束括号，手动添加
        if (endIndex < 0) {
            qDebug() << "JSON数组未终止，尝试手动添加结束括号";
            jsonData.append(']');
            endIndex = jsonData.size() - 1;
        }
    } else if (jsonData.indexOf('{', startIndex) == startIndex) {
        // 寻找匹配的 }
        int openBraces = 1;
        for (int i = startIndex + 1; i < jsonData.size(); i++) {
            char c = jsonData[i];
            if (c == '{') openBraces++;
            else if (c == '}') {
                openBraces--;
                if (openBraces == 0) {
                    endIndex = i;
                    break;
                }
            }
        }
        
        // 如果找不到结束括号，手动添加
        if (endIndex < 0) {
            qDebug() << "JSON对象未终止，尝试手动添加结束括号";
            jsonData.append('}');
            endIndex = jsonData.size() - 1;
        }
    }
    
    if (endIndex < 0) {
        // 如果找不到正确的结束位置，尝试查找最后一个 } 或 ]
        int closeBrace = jsonData.lastIndexOf('}');
        int closeBracket = jsonData.lastIndexOf(']');
        endIndex = qMax(closeBrace, closeBracket);
        
        if (endIndex < startIndex) {
            qDebug() << "无法找到JSON结束位置";
            emit error("解析文件列表失败: 无法找到JSON结束位置");
            return;
        }
    }
    
    // 提取JSON部分
    jsonData = jsonData.mid(startIndex, endIndex - startIndex + 1);
    qDebug() << "提取的JSON数据:" << jsonData;
    
    // 尝试清理JSON，处理尾部可能的垃圾数据
    if (jsonData.endsWith(",]")) {
        jsonData.replace(",]", "]");
    }
    if (jsonData.contains("}{")) {
        int validEnd = jsonData.indexOf("}{");
        if (validEnd > 0) {
            jsonData = jsonData.left(validEnd + 1);
        }
    }
    
    // 处理JSON数组中的错误格式
    QByteArray cleanedJson = jsonData;
    cleanedJson.replace(",]", "]");  // 移除数组末尾多余的逗号
    cleanedJson.replace(",}", "}");  // 移除对象末尾多余的逗号
    
    // 解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(cleanedJson, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QString errorMsg = QString("解析文件列表失败: %1").arg(parseError.errorString());
        qDebug() << errorMsg << "，错误位置:" << parseError.offset;
        
        // 显示原始数据用于调试
        QString hexString;
        qsizetype maxChars = qMin(cleanedJson.size(), qsizetype(100));
        for (qsizetype i = 0; i < maxChars; ++i) {
            hexString += QString("%1 ").arg((unsigned char)cleanedJson.at(i), 2, 16, QChar('0'));
        }
        qDebug() << "JSON数据(十六进制):" << hexString;
        
        // 尝试使用特殊处理来解析
        fileList = parseFileListManually(cleanedJson);
        if (!fileList.isEmpty()) {
            qDebug() << "通过手动解析成功获取了" << fileList.size() << "个文件";
            emit fileListReceived(fileList);
            return;
        }
        
        emit error(errorMsg);
        return;
    }
    
    // 处理不同的JSON格式
    if (doc.isArray()) {
        // 直接是文件数组
        QJsonArray array = doc.array();
        for (int i = 0; i < array.size(); i++) {
            QJsonValue value = array[i];
            if (value.isObject()) {
                QJsonObject obj = value.toObject();
                if (obj.contains("name") && obj.contains("size")) {
                    QString name = obj["name"].toString();
                    qint64 size = obj["size"].toVariant().toLongLong();
                    fileList.append(qMakePair(name, size));
                }
            }
        }
    } else if (doc.isObject()) {
        // 对象格式，查找files数组
        QJsonObject obj = doc.object();
        if (obj.contains("files") && obj["files"].isArray()) {
            QJsonArray array = obj["files"].toArray();
            for (int i = 0; i < array.size(); i++) {
                QJsonValue value = array[i];
                if (value.isObject()) {
                    QJsonObject fileObj = value.toObject();
                    if (fileObj.contains("name") && fileObj.contains("size")) {
                        QString name = fileObj["name"].toString();
                        qint64 size = fileObj["size"].toVariant().toLongLong();
                        fileList.append(qMakePair(name, size));
                    }
                }
            }
        } else {
            // 尝试解析单个文件对象
            if (obj.contains("name") && obj.contains("size")) {
                QString name = obj["name"].toString();
                qint64 size = obj["size"].toVariant().toLongLong();
                fileList.append(qMakePair(name, size));
            }
        }
    }
    
    qDebug() << "接收到文件列表，共" << fileList.size() << "个文件";
    emit fileListReceived(fileList);
}

// 手动解析文件列表（应对极端情况）
QList<QPair<QString, qint64>> TcpClient::parseFileListManually(const QByteArray &data)
{
    QList<QPair<QString, qint64>> result;
    
    // 查找所有"name":和"size":对
    QByteArray dataStr = data;
    int pos = 0;
    
    while ((pos = dataStr.indexOf("\"name\":", pos)) != -1) {
        int nameStart = dataStr.indexOf('"', pos + 7) + 1;
        if (nameStart <= 0) break;
        
        int nameEnd = dataStr.indexOf('"', nameStart);
        if (nameEnd <= 0) break;
        
        QString fileName = QString::fromUtf8(dataStr.mid(nameStart, nameEnd - nameStart));
        
        int sizePos = dataStr.indexOf("\"size\":", nameEnd);
        if (sizePos == -1) break;
        
        int sizeStart = sizePos + 7;
        int sizeEnd = dataStr.indexOf(',', sizeStart);
        if (sizeEnd == -1) {
            sizeEnd = dataStr.indexOf('}', sizeStart);
        }
        if (sizeEnd == -1) break;
        
        bool ok;
        qint64 fileSize = dataStr.mid(sizeStart, sizeEnd - sizeStart).trimmed().toLongLong(&ok);
        if (!ok) {
            pos = nameEnd + 1;
            continue;
        }
        
        result.append(qMakePair(fileName, fileSize));
        pos = nameEnd + 1;
    }
    
    return result;
}

void TcpClient::onBytesWritten(qint64 bytes)
{
    // 可以在这里添加写入数据的跟踪
    // qDebug() << "已写入" << bytes << "字节数据到服务器";
}

void TcpClient::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMessage;
    
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        errorMessage = "远程主机关闭了连接";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMessage = "找不到服务器";
        break;
    case QAbstractSocket::ConnectionRefusedError:
        errorMessage = "连接被拒绝";
        break;
    case QAbstractSocket::SocketTimeoutError:
        errorMessage = "连接超时";
        break;
    case QAbstractSocket::NetworkError:
        errorMessage = "网络错误";
        break;
    default:
        errorMessage = QString("发生错误: %1").arg(m_socket->errorString());
    }
    
    qDebug() << "Socket错误:" << errorMessage;
    emit error(errorMessage);
}

void TcpClient::finishFileDownload()
{
    qDebug() << "文件 " << m_currentFileName << " 下载完成, 共接收 " << m_bytesReceived << " 字节";
    
    // 保存文件名和大小，因为在清理之后m_file将被设置为nullptr
    QString fileName = m_currentFileName;
    qint64 fileSize = m_bytesReceived;
    
    // 关闭文件
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    
    m_currentFileName.clear();
    m_fileSize = 0;
    m_bytesReceived = 0;
    
    // 发送文件接收完成信号
    emit fileReceived(fileName, fileSize);
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