#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "TestNxFileServer.h"

TestFileServer::TestFileServer(const QString &shareDir, quint16 port, QObject *parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
    , m_shareDir(shareDir)
{
    // 连接信号
    connect(m_server, &QTcpServer::newConnection, this, &TestFileServer::handleNewConnection);
    
    // 开始监听
    if (!m_server->listen(QHostAddress::Any, port)) {
        qCritical() << "无法在端口" << port << "上启动服务器:" << m_server->errorString();
        return;
    }
    
    qInfo() << "NX文件服务器启动在端口" << port << "，共享目录:" << m_shareDir;
    qInfo() << "可用文件:";
    for (const auto &fileInfo : getFileList()) {
        qInfo() << "  -" << fileInfo.first << "(" << fileInfo.second << "字节)";
    }
}

TestFileServer::~TestFileServer() {
    // 清理所有客户端连接
    for (QTcpSocket *socket : m_clients) {
        socket->disconnectFromHost();
    }
    
    m_server->close();
}

void TestFileServer::handleNewConnection() {
    QTcpSocket *clientSocket = m_server->nextPendingConnection();
    if (!clientSocket) {
        return;
    }
    
    qInfo() << "新客户端连接:" << clientSocket->peerAddress().toString();
    
    // 连接信号
    connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
        handleReadyRead(clientSocket);
    });
    
    connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
        qInfo() << "客户端断开连接:" << clientSocket->peerAddress().toString();
        m_clients.removeOne(clientSocket);
        clientSocket->deleteLater();
    });
    
    m_clients.append(clientSocket);
}

void TestFileServer::handleReadyRead(QTcpSocket *socket) {
    QByteArray data = socket->readAll();
    
    if (data.startsWith("LIST")) {
        sendFileList(socket);
    }
    else if (data.startsWith("GET")) {
        QString fileName = QString::fromUtf8(data.mid(4)).trimmed();
        sendFile(socket, fileName);
    }
    else {
        qWarning() << "未知命令:" << data;
    }
}

QList<QPair<QString, qint64>> TestFileServer::getFileList() {
    QList<QPair<QString, qint64>> fileList;
    QDir dir(m_shareDir);
    
    if (!dir.exists()) {
        qWarning() << "共享目录不存在:" << m_shareDir;
        return fileList;
    }
    
    QStringList files = dir.entryList(QDir::Files);
    for (const QString &fileName : files) {
        QFileInfo fileInfo(dir.filePath(fileName));
        fileList.append(qMakePair(fileName, fileInfo.size()));
    }
    
    return fileList;
}

void TestFileServer::sendFileList(QTcpSocket *socket) {
    QJsonArray fileArray;
    
    for (const auto &fileInfo : getFileList()) {
        QJsonObject fileObj;
        fileObj["name"] = fileInfo.first;
        fileObj["size"] = fileInfo.second;
        fileArray.append(fileObj);
    }
    
    QJsonDocument doc(fileArray);
    QByteArray jsonData = doc.toJson();
    
    socket->write(jsonData);
    socket->write("\n"); // 添加分隔符表示文件列表结束
    
    qInfo() << "向客户端发送文件列表:" << socket->peerAddress().toString();
}

void TestFileServer::sendFile(QTcpSocket *socket, const QString &fileName) {
    QString filePath = QDir(m_shareDir).filePath(fileName);
    QFile file(filePath);
    
    if (!file.exists()) {
        qWarning() << "请求的文件不存在:" << filePath;
        return;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件:" << filePath << "错误:" << file.errorString();
        return;
    }
    
    qint64 fileSize = file.size();
    
    // 准备文件头
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_15);
    
    // 写入标记
    stream << (qint32)0x12345678;
    
    // 写入文件名和大小
    stream << fileName << fileSize;
    
    // 填充到特定大小 (1024 字节)
    const int headerSize = 1024;
    header.resize(headerSize);
    for (int i = header.size(); i < headerSize; i++) {
        header[i] = 0;
    }
    
    // 发送文件头
    socket->write(header);
    
    // 发送文件内容
    qint64 bytesWritten = 0;
    QByteArray buffer;
    
    while (!file.atEnd()) {
        buffer = file.read(64*1024); // 每次读取 64KB
        socket->write(buffer);
        bytesWritten += buffer.size();
        socket->waitForBytesWritten();
        
        // 打印进度
        int progress = (bytesWritten * 100) / fileSize;
        qInfo() << "发送文件进度:" << fileName << progress << "%";
    }
    
    qInfo() << "文件发送完成:" << fileName;
    file.close();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("TestNxFileServer");
    QCoreApplication::setApplicationVersion("1.0");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("测试NX文件服务器");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // 添加命令行选项
    QCommandLineOption portOption(QStringList() << "p" << "port", "服务器端口 (默认: 8000)", "port", "8000");
    parser.addOption(portOption);
    
    QCommandLineOption dirOption(QStringList() << "d" << "dir", "共享目录路径 (默认: 当前目录)", "dir", QDir::currentPath());
    parser.addOption(dirOption);
    
    parser.process(app);
    
    quint16 port = parser.value(portOption).toUShort();
    QString shareDir = parser.value(dirOption);
    
    TestFileServer server(shareDir, port);
    
    return app.exec();
} 