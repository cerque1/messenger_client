#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QtWebSockets/QtWebSockets>

#include "response.h"
#include "request.h"

class MessagesProcessor;

class Client : public QObject
{
    Q_OBJECT
public:
    Client(QString server_url, std::shared_ptr<MessagesProcessor> messages_processor, QObject* parent = nullptr);

    std::shared_ptr<MessagesProcessor> GetMessagesProcessor() const {
        return messages_processor_;
    }

signals:
    void SendRequest(QByteArray message);
    void InitWebSocket();
    void NewBinaryMessage(QByteArray);

public slots:
    void SendRequestSlot(QByteArray message);
    void OnBinaryMessageReceived(QByteArray message);
    void InitWebSocketSlot();
    void OnDisconnected();

private:
    QWebSocket* socket_;
    QString server_url_;
    std::shared_ptr<MessagesProcessor> messages_processor_;
};

class MessagesProcessor : public QObject{
    Q_OBJECT
public:
    MessagesProcessor(QObject* parent = nullptr);

signals:
    void ReceiveResponse(Response);
    void ReceiveMessage(Request);

public slots:
    void OnNewMessage(QByteArray message);
    void ProcessNextMessage();

private:
    void ProcessMessage(QByteArray data);

    Response last_response_;
    QQueue<QByteArray> message_queue_;
    bool processing_message_ = false;
};

#endif // CLIENT_H
