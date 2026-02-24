#include "request.h"

QByteArray Request::getJsonRequest() const{
    QJsonObject obj;
    obj["path"] = path_;
    obj["body"] = body_;
    return QJsonDocument(obj).toJson();
}

const QJsonObject& Request::getBody() const{
    return body_;
}

const QString& Request::getPath() const{
    return path_;
}

QVariant Request::getValueFromBody(QString key) const{
    return body_[key];
}

void Request::setPath(QString path){
    path_ = path;
}

void Request::setBody(QJsonObject&& body){
    body_ = std::move(body);
}

void Request::setBody(const QJsonObject& body){
    body_ = body;
}

Request Request::fromJson(QByteArray json){
    Request request;
    QJsonObject obj = QJsonDocument::fromJson(json).object();

    request.setPath(obj["path"].toString());
    request.setBody(obj["body"].toObject());

    return request;
}
