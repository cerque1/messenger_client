#ifndef REQ_RESP_UTILS_H
#define REQ_RESP_UTILS_H

#include "request.h"
#include "response.h"
#include "entities.h"

namespace req_resp_utils{

Response SendReqAndWaitResp(Request req);

Request MakeGetChatRequest(QString token, QDateTime last_update_time);
QList<entities::Chat> MakeChatFromResponse(const Response& resp);

Request MakeGetMessagesRequest(QString token, int chat_id, QDateTime last_update_time);
entities::MessagesToChat MakeMessagesFromResponse(const Response& resp);
entities::ChatMembers MakeChatMembersFromResponse(const Response& resp);
entities::ChatMember MakeChangeChatMemberFromMessage(const Request& req);

QList<entities::Content> MakeContentsFromResponse(const Response& resp);

Request MakeCreateDialogRequest(QString token, QString nickname);
Request MakeCreateGroupChatRequest(QString token, QString name);
Request MakeDeleteChatMemberRequest(QString token, int chat_id, int member_id);

entities::UserInfoInChat MakeChatMemberFromMessage(const Request& req);

Request MakeNewStatusRequest(int chat_id, int message_id, int status, QString token);
int TakeNewChatIdFromResponse(const Response& resp);

Request MakeSendMessageRequest(QString token, int pre_id, int chat_id, QString text, int content_size);
Request MakeDeleteMessageRequest(QString token, int chat_id, int message_id, bool full_delete);
Request MakeChangeMessageRequest(QString token, int chat_id, int message_id, QString text);

entities::Chat MakeChatFromMessage(const Request& req);
entities::Message MakeMessageFromMessage(const Request& req);
entities::Status MakeStatusFromMessage(const Request& req);

}

#endif // REQ_RESP_UTILS_H
