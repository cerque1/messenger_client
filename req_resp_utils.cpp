#include "req_resp_utils.h"
#include "generaldata.h"

#include <QJsonArray>

namespace req_resp_utils{

Response SendReqAndWaitResp(Request req) {
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    bool timeout = false;
    Response resp;

    QObject::connect(&timer, &QTimer::timeout, [&]() {
        timeout = true;
        loop.quit();
    });

    QObject::connect(
        data::GeneralData::GetInstance()->GetMessagesProcessor().get(),
        &MessagesProcessor::ReceiveResponse,
        [&](Response response) {
            resp = response;
            QMetaObject::invokeMethod(&timer, "stop");
            loop.quit();
        });

    emit data::GeneralData::GetInstance()->GetClient()->SendRequest(req.getJsonRequest());

    timer.start(5000);
    loop.exec();

    QObject::disconnect(data::GeneralData::GetInstance()->GetMessagesProcessor().get(), &MessagesProcessor::ReceiveResponse, nullptr, nullptr);

    if (timeout) {
        throw std::runtime_error("No response from server");
    }

    return resp;
}


Request MakeGetChatRequest(QString token, QDateTime last_update_time){
    Request req;
    req.setPath("get_updates_chats");
    req.setValueToBody("token", token);
    req.setValueToBody("last_update_time", last_update_time.toString("yyyy-MM-dd HH:mm:ss"));
    return req;
}

QList<entities::Chat> MakeChatFromResponse(const Response& resp){
    QList<entities::Chat> chats;
    QJsonArray chats_json = resp.getValueFromBody("chats").toJsonArray();
    for(auto chat : chats_json){
        QJsonObject chat_json = chat.toObject();
        chats.push_back(entities::Chat(chat_json["id"].toInt(),
                                       chat_json["create_time"].toString(),
                                       chat_json["last_update_time"].toString(),
                                       chat_json["is_dialog"].toBool(),
                                       chat_json["name"].toString()));
    }
    return chats;
}

Request MakeGetMessagesRequest(QString token, int chat_id, QDateTime last_update_time){
    Request req;
    req.setPath("get_updates_messages_to_chat");
    req.setValueToBody("token", token);
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("last_update_time", last_update_time.toString("yyyy-MM-dd hh:mm:ss"));
    return req;
}

entities::MessagesToChat MakeMessagesFromResponse(const Response& resp){
    int chat_id = resp.getValueFromBody("chat_id").toInt();
    entities::MessagesToChat messages{resp.getValueFromBody("chat_id").toInt()};
    QJsonArray messages_json = resp.getValueFromBody("messages").toJsonArray();
    for(auto message : messages_json){
        QJsonObject message_json = message.toObject();
        auto messages_r = entities::Message(message_json["id"].toInt(),
                                            chat_id,
                                            message_json["sender_id"].toInt(),
                                            message_json["text"].toString(),
                                            message_json["create_time"].toString(),
                                            message_json["status"].toInt(),
                                            message_json["is_changed"].toBool());

        for(auto content : message_json["contents"].toArray()){
            QJsonObject content_json = content.toObject();
            messages_r.files_.push_back(content_json["filename"].toString());
        }

        messages.messages_.push_back(messages_r);
    }
    return messages;
}

entities::ChatMembers MakeChatMembersFromResponse(const Response& resp){
    auto chat_members_json = resp.getValueFromBody("chat_members").toJsonObject();
    entities::ChatMembers chat_members(chat_members_json["chat_id"].toInt());

    for(auto chat_member_json : chat_members_json["members"].toArray()){
        auto chat_member_obj = chat_member_json.toObject();
        chat_members.users_[chat_member_obj["id"].toInt()] = entities::UserInfoInChat{
            chat_member_obj["id"].toInt(),
            chat_member_obj["name"].toString(),
            chat_member_obj["birthday"].toString(),
            chat_member_obj["last_online"].toString(),
            chat_member_obj["role"].toInt()
        };
    }
    return chat_members;
}

entities::UserInfoInChat MakeChatMemberFromMessage(const Request& req){
    auto info = entities::UserInfoInChat(
            req.getValueFromBody("user_id").toInt(),
            req.getValueFromBody("user_name").toString(),
            req.getValueFromBody("birthday").toString(),
            req.getValueFromBody("last_online").toString(),
            req.getValueFromBody("role").toInt()
        );
    info.chat_id_ = req.getValueFromBody("chat_id").toInt();
    return info;
}

entities::ChatMember MakeChangeChatMemberFromMessage(const Request& req){
    return entities::ChatMember{
        req.getValueFromBody("user_id").toInt(),
        "",
        req.getValueFromBody("chat_id").toInt(),
        req.getValueFromBody("role").toInt()
    };
}

QList<entities::Content> MakeContentsFromResponse(const Response& resp){
    QList<entities::Content> contents;
    for(auto content : resp.getValueFromBody("contents").toJsonArray()){
        auto content_json = content.toObject();
        auto content_r = entities::Content(content_json["content_id"].toInt(),
                                           content_json["chat_id"].toInt(),
                                           content_json["message_id"].toInt(),
                                           content_json["filename"].toString());
        contents.push_back(content_r);
    }
    return contents;
}

Request MakeNewStatusRequest(int chat_id, int message_id, int status, QString token){
    Request req;
    req.setPath("set_status_to_message");
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("message_id", message_id);
    req.setValueToBody("status", status);
    req.setValueToBody("token", token);
    return req;
}

Request MakeCreateDialogRequest(QString token, QString nickname){
    Request req;
    req.setPath("create_dialog");
    req.setValueToBody("token", token);
    req.setValueToBody("nickname", nickname);
    return req;
}

Request MakeCreateGroupChatRequest(QString token, QString name){
    Request req;
    req.setPath("create_group_chat");
    req.setValueToBody("token", token);
    req.setValueToBody("chat_name", name);
    return req;
}

int TakeNewChatIdFromResponse(const Response& resp){
    return resp.getValueFromBody("created_chat").toInt();
}

Request MakeSendMessageRequest(QString token, int pre_id, int chat_id, QString text, int content_size){
    Request req;
    req.setPath("send_message");
    req.setValueToBody("pre_id", pre_id);
    req.setValueToBody("token", token);
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("text", text);
    req.setValueToBody("content_size", content_size);

    return req;
}

Request MakeDeleteMessageRequest(QString token, int chat_id, int message_id,  bool full_delete){
    Request req;
    req.setPath("delete_message");
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("message_id", message_id);
    req.setValueToBody("full_delete",  full_delete);
    req.setValueToBody("token", token);
    return req;
}

Request MakeChangeMessageRequest(QString token, int chat_id, int message_id, QString text){
    Request req;
    req.setPath("change_message");
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("message_id", message_id);
    req.setValueToBody("text",  text);
    req.setValueToBody("token", token);
    return req;
}

Request MakeDeleteChatMemberRequest(QString token, int chat_id, int member_id) {
    Request req;
    req.setPath("delete_member_from_chat");
    req.setValueToBody("chat_id", chat_id);
    req.setValueToBody("user_id", member_id);
    req.setValueToBody("token", token);
    return req;
}

entities::Chat MakeChatFromMessage(const Request& req){
    entities::Chat chat;
    chat.id_ = req.getValueFromBody("chat_id").toInt();
    chat.create_time_ = req.getValueFromBody("create_time").toString();
    chat.last_update_time_ = req.getValueFromBody("last_update_time").toString();
    chat.is_dialog_ = req.getValueFromBody("is_dialog").toBool();
    chat.name_ = req.getValueFromBody("name").toString();
    return chat;
}

entities::Message MakeMessageFromMessage(const Request& req){
    entities::Message message;
    message.id_ = req.getValueFromBody("message_id").toInt();
    message.chat_id_ = req.getValueFromBody("chat_id").toInt();
    message.sender_id_ = req.getValueFromBody("sender_id").toInt();
    message.text_ = req.getValueFromBody("text").toString();
    message.create_time_ = req.getValueFromBody("create_time").toString();
    message.is_changed_ = req.getValueFromBody("is_changed").toBool();
    message.status_ = req.getValueFromBody("status").toInt();

    for(const auto& content : req.getValueFromBody("contents").toJsonArray()){
        message.files_.push_back(content.toObject()["filename"].toString());
    }

    return message;
}

entities::Status MakeStatusFromMessage(const Request& req){
    entities::Status status;
    status.chat_id_ = req.getValueFromBody("chat_id").toInt();
    status.message_id_ = req.getValueFromBody("message_id").toInt();
    status.user_id_ = req.getValueFromBody("user_id").toInt();
    status.status_ = req.getValueFromBody("status").toInt();
    return status;
}

}

