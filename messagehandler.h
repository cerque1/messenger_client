#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H

#include <QObject>
#include "entities.h"
#include "request.h"

class MessageHandler : public QObject
{
    Q_OBJECT
public:
    explicit MessageHandler(QObject *parent = nullptr);

public slots:
    void Handle(Request message);

signals:
    void NewChat(entities::Chat);
    void AddChatMember(entities::UserInfoInChat);
    void ChangeMemberRole(entities::ChatMember);
    void NewMessage(entities::Message);
    void DeleteMessage(int chat_id, int message_id);
    void UpdateMessageStatus(entities::Status);
    void ChangeMessage(int chat_id, int message_id, QString text);
    void DeleteChatMember(int chat_id, int member_id);
    void IncomingCall(int chat_id, int caller_id, QString caller_name, QString offer_sdp);
    void CallAccepted(int chat_id, QString answer_sdp);
    void CallDeclined(int chat_id);
    void CallEnded(int chat_id);
    void CallCandidate(int chat_id, QString candidate, QString mid, int mline_index);
};

#endif // MESSAGEHANDLER_H
