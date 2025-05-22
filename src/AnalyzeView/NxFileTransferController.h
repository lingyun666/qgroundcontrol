#pragma once

#include <QObject>
#include <QFileInfo>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QStringList>

class TcpClient;

class NxFileTransferController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool        isConnected     READ isConnected    NOTIFY connectionStateChanged)
    Q_PROPERTY(bool        isDownloading   READ isDownloading  NOTIFY downloadStatusChanged)
    Q_PROPERTY(QString     statusMessage   READ statusMessage  NOTIFY statusMessageChanged)
    Q_PROPERTY(int         progress        READ progress       NOTIFY progressChanged)
    Q_PROPERTY(QStringList fileList        READ fileList       NOTIFY fileListChanged)
    Q_PROPERTY(QStringList downloadedFiles READ downloadedFiles NOTIFY downloadedFilesChanged)
    Q_PROPERTY(QString     downloadPath    READ downloadPath   NOTIFY downloadPathChanged)

public:
    explicit NxFileTransferController(QObject* parent = nullptr);
    ~NxFileTransferController() override;

    // Q_INVOKABLE 方法可以从 QML 调用
    Q_INVOKABLE void connectToServer(const QString& host, int port = 8000);
    Q_INVOKABLE void disconnectFromServer();
    Q_INVOKABLE void refreshFileList();
    Q_INVOKABLE void downloadFile(const QString& fileName, const QString& savePath = QString());
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void openDownloadFolder();
    Q_INVOKABLE void deleteDownloadedFile(const QString& fileName);
    Q_INVOKABLE void clearDownloadedFiles();

    // 属性访问方法
    bool isConnected() const;
    bool isDownloading() const { return m_isDownloading; }
    QString statusMessage() const { return m_statusMessage; }
    int progress() const { return m_progress; }
    QStringList fileList() const;
    QStringList downloadedFiles() const { return m_downloadedFiles; }
    QString downloadPath() const { return m_downloadPath; }

signals:
    void connectionStateChanged(bool connected);
    void downloadStatusChanged(bool downloading);
    void statusMessageChanged(QString message);
    void progressChanged(int progress);
    void fileListChanged();
    void fileDownloaded(QString fileName, qint64 size);
    void errorMessage(QString message);
    void downloadedFilesChanged();
    void downloadPathChanged();

private slots:
    void _onConnectionStateChanged(bool connected);
    void _onFileListReceived(const QList<QPair<QString, qint64>>& fileList);
    void _onDownloadProgress(const QString& fileName, int progress);
    void _onFileReceived(const QString& fileName, qint64 size);
    void _onError(const QString& errorMsg);

private:
    void _updateStatusMessage(const QString& message);
    void _updateDownloadPath();

    TcpClient* m_tcpClient;
    bool m_isDownloading;
    QString m_statusMessage;
    int m_progress;
    QList<QPair<QString, qint64>> m_fileList;
    QStringList m_downloadedFiles;
    QString m_downloadPath;
}; 