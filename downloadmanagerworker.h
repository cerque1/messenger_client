#ifndef DOWNLOADMANAGERWORKER_H
#define DOWNLOADMANAGERWORKER_H

#include <QObject>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include "filedownloadmanager.h"

struct DownloadTask {
    int messageId;
    int fileIndex;
    QString filename;
};

class DownloadManagerWorker : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManagerWorker(const QString& url)
        : url_(url), thread_(new QThread)
    {
        manager_ = new FileDownloadManager(url_);
        manager_->moveToThread(thread_);

        connect(this, &DownloadManagerWorker::startDownload,
                manager_, &FileDownloadManager::downloadFile);

        connect(manager_, &FileDownloadManager::downloadCompleted,
                this, &DownloadManagerWorker::onDownloadFinished);

        connect(manager_, &FileDownloadManager::downloadFailed,
                this, &DownloadManagerWorker::onDownloadFinished);

        thread_->start();
    }

    ~DownloadManagerWorker() {
        thread_->quit();
        thread_->wait();
        delete manager_;
        delete thread_;
    }

    FileDownloadManager* GetFileManager() {
        return manager_;
    }

signals:
    void startDownload(int messageId, int fileIndex, const QString& filename);

public slots:
    void enqueueDownload(int messageId, int fileIndex, const QString& filename)
    {
        QMutexLocker locker(&mutex_);

        DownloadTask task{messageId, fileIndex, filename};
        queue_.enqueue(task);

        if (!isDownloading_) {
            processNext();
        }
    }

private slots:
    void onDownloadFinished(int, int)
    {
        QMutexLocker locker(&mutex_);
        isDownloading_ = false;
        processNext();
    }

private:
    void processNext()
    {
        if (queue_.isEmpty()) {
            isDownloading_ = false;
            return;
        }

        isDownloading_ = true;
        DownloadTask task = queue_.dequeue();

        emit startDownload(task.messageId,
                           task.fileIndex,
                           task.filename);
    }

private:
    QString url_;
    FileDownloadManager* manager_;
    QThread* thread_;

    QQueue<DownloadTask> queue_;
    QMutex mutex_;
    bool isDownloading_ = false;
};

#endif
