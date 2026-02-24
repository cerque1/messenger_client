#include "messagehandler.h"
#include "req_resp_utils.h"
#include "request.h"
#include "generaldata.h"

MessageHandler::MessageHandler(QObject *parent)
    : QObject{parent}
{
    connect(&*data::GeneralData::GetInstance()->GetMessagesProcessor(), SIGNAL(ReceiveMessage(Request)), this, SLOT(Handle(Request)));
}

void MessageHandler::Handle(Request message){
    QString path = message.getPath();
    qDebug() << message.getJsonRequest();
    if(path == "create_chat"){
        emit NewChat(req_resp_utils::MakeChatFromMessage(message));
    }
    else if(path == "send_message"){
        emit NewMessage(req_resp_utils::MakeMessageFromMessage(message));
    }
    else if(path == "new_status"){
        emit UpdateMessageStatus(req_resp_utils::MakeStatusFromMessage(message));
    }
    else if(path == "add_chat_member"){
        emit AddChatMember(req_resp_utils::MakeChatMemberFromMessage(message));
    }
    else if(path == "change_member_role"){
        emit ChangeMemberRole(req_resp_utils::MakeChangeChatMemberFromMessage(message));
    }
    else if(path == "delete_chat_member"){
        emit DeleteChatMember(message.getValueFromBody("chat_id").toInt(),
                              message.getValueFromBody("user_id").toInt());
    }
    else if(path == "delete_message"){
        emit DeleteMessage(message.getValueFromBody("chat_id").toInt(), message.getValueFromBody("message_id").toInt());
    }
    else if(path == "change_message") {
        emit ChangeMessage(message.getValueFromBody("chat_id").toInt(),
                           message.getValueFromBody("message_id").toInt(),
                           message.getValueFromBody("text").toString());
    }
    else if(path == "delete_chat_member") {
        emit DeleteChatMember(message.getValueFromBody("chat_id").toInt(),
                              message.getValueFromBody("user_id").toInt());
    }
    else if(path == "incoming_call") {
        emit IncomingCall(message.getValueFromBody("chat_id").toInt(),
                          message.getValueFromBody("caller_id").toInt(),
                          message.getValueFromBody("caller_name").toString(),
                          message.getValueFromBody("sdp").toString());
    }
    else if(path == "call_answer") {
        emit CallAccepted(message.getValueFromBody("chat_id").toInt(),
                          message.getValueFromBody("sdp").toString());
    }
    else if(path == "call_declined") {
        emit CallDeclined(message.getValueFromBody("chat_id").toInt());
    }
    else if(path == "call_ended") {
        emit CallEnded(message.getValueFromBody("chat_id").toInt());
    }
    else if(path == "call_candidate") {
        emit CallCandidate(message.getValueFromBody("chat_id").toInt(),
                           message.getValueFromBody("candidate").toString(),
                           message.getValueFromBody("mid").toString(),
                           message.getValueFromBody("mline_index").toInt());
    }
}
