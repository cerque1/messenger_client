#include "filedownloadmanager.h"
#include <QDir>
#include "generaldata.h"
#include "request.h"

FileDownloadManager::FileDownloadManager(QString url, QObject* parent)
    : QObject(parent), url_(url) {}

void FileDownloadManager::initSocket()
{
    websocket_ = new QWebSocket();
    QEventLoop loop;
    connect(websocket_, SIGNAL(connected()), &loop, SLOT(quit()));
    connect(websocket_, &QWebSocket::binaryMessageReceived,
            this, &FileDownloadManager::onSocketBinaryMessageReceived);

    websocket_->open(QUrl(url_));
     loop.exec();
}

void FileDownloadManager::downloadFile(int messageId,
                                       int fileIndex,
                                       const QString& filename)
{
    QMutexLocker locker(&mutex_);

    if (!websocket_)
        initSocket();

    currentFile_ = {};
    currentFile_.filename = filename;
    currentFile_.messageId = messageId;
    currentFile_.fileIndex = fileIndex;

    QDir dir("temp/files");
    if (!dir.exists())
        dir.mkpath(".");

    currentFile_.localPath = dir.absoluteFilePath(filename);

    fileHandle_ = std::make_unique<QFile>(currentFile_.localPath);
    if (!fileHandle_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit downloadFailed(messageId, fileIndex);
        return;
    }

    sendGetFileRequest();
}

void FileDownloadManager::sendGetFileRequest()
{
    Request request;
    request.setPath("get_file");
    request.setValueToBody("filename", currentFile_.filename);
    request.setValueToBody("token", getToken());

    websocket_->sendBinaryMessage(request.getJsonRequest());
}

void FileDownloadManager::onSocketBinaryMessageReceived(const QByteArray& data)
{
    Request req = Request::fromJson(data);

    if (req.getPath() != "file")
        return;

    QString filename = req.getValueFromBody("filename").toString();
    if (filename != currentFile_.filename)
        return;

    if (currentFile_.canceled)
        return;

    qint64 fullSize = req.getValueFromBody("full_size").toLongLong();
    QString base64String = req.getValueFromBody("data").toString();
    QByteArray chunk = QByteArray::fromBase64(base64String.toLatin1());
    QByteArray raw = req.getValueFromBody("data").toString().toUtf8();
    qDebug() << "Base64 size:" << raw.size();
    qDebug() << "Decoded size:" << QByteArray::fromBase64(raw).size();

    if (currentFile_.totalBytes == 0)
        currentFile_.totalBytes = fullSize;

    fileHandle_->write(chunk);
    currentFile_.receivedBytes += chunk.size();

    emit downloadProgress(currentFile_.messageId,
                          currentFile_.fileIndex,
                          currentFile_.receivedBytes,
                          currentFile_.totalBytes);

    if (currentFile_.receivedBytes >= currentFile_.totalBytes) {
        finishDownload(true);
    }
}

void FileDownloadManager::finishDownload(bool success)
{
    if (fileHandle_) {
        fileHandle_->close();
        fileHandle_.reset();
    }

    if (success) {
        emit downloadCompleted(currentFile_.messageId,
                               currentFile_.fileIndex);
    } else {
        emit downloadFailed(currentFile_.messageId,
                            currentFile_.fileIndex);
    }

    currentFile_ = {};
}

void FileDownloadManager::cancelCurrentDownload()
{
    QMutexLocker locker(&mutex_);

    currentFile_.canceled = true;

    if (fileHandle_) {
        fileHandle_->close();
        fileHandle_.reset();
    }

    emit downloadFailed(currentFile_.messageId,
                        currentFile_.fileIndex);

    currentFile_ = {};
}

QString FileDownloadManager::getToken()
{
    return data::GeneralData::GetInstance()->GetToken();
}
