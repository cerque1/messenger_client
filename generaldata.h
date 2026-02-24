#ifndef GENERALDATA_H
#define GENERALDATA_H

#include "client.h"
#include "entities.h"

#include <QString>
#include <QDateTime>
#include <unordered_map>

namespace data{

class GeneralData
{
public:
    static GeneralData* GetInstance(){
        static std::unique_ptr<GeneralData> instance{new GeneralData()};
        return &*instance;
    }

    void SetToken(QString token){
        token_ = token;
    }

    QString GetToken() const {
        return token_;
    }

    void SetLastUpdateTime(QDateTime last_upadate_time){
        last_update_time_ = last_upadate_time;
    }

    QDateTime GetLastUpdateTime() const {
        return last_update_time_;
    }

    void SetClient(std::shared_ptr<Client> client){
        client_ = client;
        m_processor_ = client_->GetMessagesProcessor();
    }

    std::shared_ptr<Client> GetClient(){
        return client_;
    }

    std::shared_ptr<MessagesProcessor> GetMessagesProcessor(){
        return m_processor_;
    }

    void SetUserId(int current_user_id){
        current_user_id_ = current_user_id;
    }

    int GetUserId() const {
        return current_user_id_;
    }

    void SetUserName(QString name) {
        user_name_ = name;
    }

    QString GetUserName() const {
        return user_name_;
    }

    void SetLastChatUpdateTime(int chat_id, QDateTime last_update_time){
        chats_to_last_update_message_time_[chat_id] = last_update_time;
    }

    QDateTime GetLastChatUpdateTime(int chat_id) const {
        if(chats_to_last_update_message_time_.find(chat_id) == chats_to_last_update_message_time_.end()){
            QDateTime date = QDateTime::fromString("2025-01-01 00:00:00", "yyyy-MM-dd hh:mm:ss");
            return date;
        }
        return chats_to_last_update_message_time_.at(chat_id);
    }

    void AddChatMembersToChat(int chat_id, entities::ChatMembers chat_members){
        chats_members_[chat_id] = chat_members;
    }

    entities::ChatMembers* getMembersToChat(int chat_id){
        if(chats_members_.find(chat_id) == chats_members_.end()){
            return nullptr;
        }
        return &chats_members_[chat_id];
    }

    GeneralData(const GeneralData&) = delete;
    GeneralData& operator=(const GeneralData&) = delete;

    void Clear(){
        SetToken("");
        SetUserName("");
        SetUserId(0);
        SetLastUpdateTime(QDateTime::fromString("2025-01-01 00:00:00", "yyyy-MM-dd hh:mm:ss"));
        chats_to_last_update_message_time_.clear();
        chats_members_.clear();
    }

private:
    GeneralData() = default;
    static std::unique_ptr<GeneralData> instance;

    QString token_;
    int current_user_id_;
    QString user_name_;
    QDateTime last_update_time_ = QDateTime::fromString("2025-01-01 00:00:00", "yyyy-MM-dd hh:mm:ss");
    std::unordered_map<int, QDateTime> chats_to_last_update_message_time_;
    std::unordered_map<int, entities::ChatMembers> chats_members_;
    std::shared_ptr<Client> client_;
    std::shared_ptr<MessagesProcessor> m_processor_;
};

}

#endif // GENERALDATA_H
