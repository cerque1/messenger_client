#ifndef UPDATER_H
#define UPDATER_H

#include <QObject>
#include <QTimer>
#include "entities.h"
#include "response.h"
#include "generaldata.h"
#include "req_resp_utils.h"

class UpdateWorker : public QObject {
    Q_OBJECT
public:
    UpdateWorker() = default;

signals:
    void Update(QList<entities::Chat>, QDateTime);
    void finished();

private slots:
    void process() {
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(onTimeout()));
        timer->start(5000);
    }

    void onTimeout() {
        QDateTime last_update_time = QDateTime::currentDateTime();
        Request req = req_resp_utils::MakeGetChatRequest(data::GeneralData::GetInstance()->GetToken(),
                                                         data::GeneralData::GetInstance()->GetLastUpdateTime().toUTC());
        qDebug() << req.getJsonRequest();
        Response resp = Response::fromJson(data::GeneralData::GetInstance()->GetClient()->SendRequest(req.getJsonRequest()));
        QList<entities::Chat> chats = req_resp_utils::MakeChatFromResponse(resp);
        if(!chats.isEmpty()){
            emit Update(req_resp_utils::MakeChatFromResponse(resp), last_update_time);
        }
        timer->start(5000);
    }

    void stop(){
        timer->stop();
        emit finished();
    }
private:
    QTimer* timer;
};

class Updater : public QObject
{
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);
    ~Updater();

signals:
    void stop();
    void UpdateUI(QList<entities::Chat>, QDateTime);
};

#endif // UPDATER_H
