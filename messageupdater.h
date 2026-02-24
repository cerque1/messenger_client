#ifndef MESSAGEUPDATER_H
#define MESSAGEUPDATER_H

#include <QObject>
#include <QTimer>
#include "entities.h"
#include "response.h"
#include "generaldata.h"
#include "req_resp_utils.h"
#include "utils.h"

class UpdateMessageWorker : public QObject {
    Q_OBJECT
public:
    UpdateMessageWorker() = default;

signals:
    void Update(entities::MessagesToChat);
    void finished();

private slots:
    void process() {
        if(timer == nullptr){
            timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
        }
        timer->start(500);
    }

    void onTimeout() {
        if(chat_id_ != -1){
            QDateTime last_update_time = QDateTime::currentDateTime();
            qDebug() << chat_id_ << data::GeneralData::GetInstance()->GetLastChatUpdateTime(chat_id_);
            Request req = req_resp_utils::MakeGetMessagesRequest(data::GeneralData::GetInstance()->GetToken(),
                                                                 chat_id_,
                                                                 data::GeneralData::GetInstance()->GetLastChatUpdateTime(chat_id_).toUTC());
            qDebug() << req.getJsonRequest();
            Response resp = data::GeneralData::GetInstance()->GetClient()->SendRequest(req.getJsonRequest());
            if(resp.getStatus() != 200){
                utils::MakeMessageBox(resp.getValueFromBody("message").toString());
                return;
            }

            emit Update(req_resp_utils::MakeMessagesFromResponse(resp));
            data::GeneralData::GetInstance()->SetLastChatUpdateTime(chat_id_, last_update_time);
        }
        timer->start(500);
    }

    void pause(){
        timer->stop();
    }

    void stop(){
        timer->stop();
        emit finished();
    }

    void setChatId(int chat_id){
        chat_id_ = chat_id;
    }

private:
    QTimer* timer;
    int chat_id_ = -1;
};

class MessageUpdater : public QObject
{
    Q_OBJECT
public:
    explicit MessageUpdater(QObject *parent = nullptr);
    ~MessageUpdater();

signals:
    void pause();
    void start_timer();
    void stop();
    void UpdateMessagesUi(entities::MessagesToChat);
    void setChatId(int chat_id);
};

#endif // MESSAGEUPDATER_H
