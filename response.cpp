#include "response.h"

int Response::getStatus() const{
    return status_;
}

const QJsonObject& Response::getBody() const{
    return body_;
}

QVariant Response::getValueFromBody(QString key) const{
    return body_[key];
}

void Response::setStatus(int status){
    status_ = status;
}

void Response::setBody(QJsonObject&& body){
    body_ = std::move(body);
}

void Response::setBody(const QJsonObject& body){
    body_ = body;
}

// мб можно по ссылке надо проверить
Response Response::fromJson(QByteArray json){
    Response response;
    QJsonObject obj = QJsonDocument::fromJson(json).object();

    response.setStatus(obj["status"].toInt());
    response.setBody(obj["body"].toObject());

    return response;
}

QByteArray Response::getJsonResponse(){
    QJsonObject obj;

    obj["status"] = status_;
    obj["body"] = body_;

    return QJsonDocument(obj).toJson();
}

