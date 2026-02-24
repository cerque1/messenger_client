#ifndef RESPONSE_H
#define RESPONSE_H

#include <QJsonObject>

class Response
{
public:
    Response() = default;

    int getStatus() const;
    const QJsonObject& getBody() const;
    QVariant getValueFromBody(QString key) const;

    void setStatus(int status);
    template <typename T>
    void setValueToBody(QString key, T&& value){
        body_[key] = std::forward<T>(value);
    }
    void setBody(QJsonObject&& body);
    void setBody(const QJsonObject& body);

    static Response fromJson(QByteArray json);
    QByteArray getJsonResponse();

private:
    int status_;
    QJsonObject body_;
};

#endif // RESPONSE_H
