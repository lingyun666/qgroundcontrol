#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>

// 简单的TCP文件服务器用于测试NX文件传输功能
class TestFileServer : public QObject
{
    Q_OBJECT
public:
    TestFileServer(const QString &shareDir, quint16 port, QObject *parent = nullptr);
    ~TestFileServer();
    
private slots:
    void handleNewConnection();
    void handleReadyRead(QTcpSocket *socket);
    
private:
    QList<QPair<QString, qint64>> getFileList();
    void sendFileList(QTcpSocket *socket);
    void sendFile(QTcpSocket *socket, const QString &fileName);
    
    QTcpServer* m_server;
    QList<QTcpSocket*> m_clients;
    QString m_shareDir;
}; 