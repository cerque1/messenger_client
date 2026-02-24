#ifndef ENTITIES_H
#define ENTITIES_H

#include <QString>
#include <QList>

namespace entities {

struct Chat{
    Chat() = default;
    Chat(int id, QString create_time, QString last_update_time, bool is_dialog, QString name)
        : id_(id), create_time_(create_time), last_update_time_(last_update_time), is_dialog_(is_dialog), name_(name){}

    int id_;
    QString create_time_;
    QString last_update_time_;
    bool is_dialog_;
    QString name_;
};

struct ChatInfo{
    ChatInfo() = default;
    ChatInfo(int chat_id, QString name, QString time)
        : chat_id_(chat_id), name_(name), time_(time){}

    int chat_id_;
    QString name_;
    QString time_;
};

struct Message{
    Message() = default;
    Message(int id, int chat_id, int sender_id, QString text, QString create_time, int status, bool is_changed, QList<QString> files = {})
        : id_(id), chat_id_(chat_id), sender_id_(sender_id), text_(text), create_time_(create_time), status_(status), is_changed_(is_changed), files_(files){}

    int id_;
    int chat_id_;
    int sender_id_;
    QString text_;
    QString create_time_;
    int status_;
    bool is_changed_;
    QList<QString> files_;
};


struct MessagesToChat{
    MessagesToChat() = default;
    MessagesToChat(int id)
        : id_(id){}

    int id_;
    QList<Message> messages_;
};

struct Status{
    Status() = default;
    Status(int chat_id, int message_id, int user_id, int status)
        : chat_id_(chat_id), message_id_(message_id), user_id_(user_id), status_(status){}

    int chat_id_;
    int message_id_;
    int user_id_;
    int status_;
};

struct Content{
    Content() = default;
    Content(int content_id, int chat_id, int message_id, QString filename)
        :content_id_(content_id), chat_id_(chat_id), message_id_(message_id), filename_(filename){}

    int content_id_;
    int chat_id_;
    int message_id_;
    QString filename_;
};

struct ChatMember{
    ChatMember() = default;
    ChatMember(int user_id, QString user_name, int chat_id, int role)
        : user_id_(user_id), user_name_(user_name), chat_id_(chat_id), role_(role){}

    int user_id_;
    QString user_name_;
    int chat_id_;
    int role_;
};

struct UserInfo{
    UserInfo() = default;
    UserInfo(int id, QString name, QString birth_day, QString last_online)
        : id_(id), name_(name), birth_day_(birth_day), last_online_(last_online){}

    int id_;
    QString name_;
    QString birth_day_;
    QString last_online_;
};

struct UserInfoInChat : UserInfo{
    UserInfoInChat() = default;
    UserInfoInChat(int id, QString name, QString birth_day, QString last_online, int role)
        : UserInfo(id, name, birth_day, last_online), role_(role){}

    int chat_id_;
    int role_;
};

struct ChatMembers{
    ChatMembers() = default;
    ChatMembers(int chat_id)
        : chat_id_(chat_id){}

    int chat_id_;
    std::unordered_map<int, UserInfoInChat> users_;
};

}

#endif // ENTITIES_H
