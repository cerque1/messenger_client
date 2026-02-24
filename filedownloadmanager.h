#ifndef FILEDOWNLOADMANAGER_H
#define FILEDOWNLOADMANAGER_H

#include <QObject>
#include <QWebSocket>
#include <QFile>
#include <QMutex>

struct DownloadFileInfo {
    QString filename;
    QString localPath;
    int messageId = -1;
    int fileIndex = -1;
    qint64 totalBytes = 0;
    qint64 receivedBytes = 0;
    bool canceled = false;
};

class FileDownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit FileDownloadManager(QString url, QObject* parent = nullptr);

    void downloadFile(int messageId, int fileIndex, const QString& filename);
    void cancelCurrentDownload();

signals:
    void downloadProgress(int messageId, int fileIndex,
                          qint64 received, qint64 total);
    void downloadCompleted(int messageId, int fileIndex);
    void downloadFailed(int messageId, int fileIndex);

private slots:
    void onSocketBinaryMessageReceived(const QByteArray& data);

private:
    void initSocket();
    void sendGetFileRequest();
    void finishDownload(bool success);
    QString getToken();

private:
    QWebSocket* websocket_ = nullptr;
    QString url_;

    DownloadFileInfo currentFile_;
    std::unique_ptr<QFile> fileHandle_;

    QMutex mutex_;
};

#endif
