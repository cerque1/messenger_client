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
};

#endif // MESSAGEHANDLER_H
