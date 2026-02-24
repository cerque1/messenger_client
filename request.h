#ifndef REQUEST_H
#define REQUEST_H

#include <QObject>
#include <QString>
#include <QJsonObject>

class Request {
public:
    Request() = default;

    QByteArray getJsonRequest() const;
    const QJsonObject& getBody() const;
    const QString& getPath() const;
    QVariant getValueFromBody(QString key) const;

    void setPath(QString path);
    template <typename T>
    void setValueToBody(QString key, T&& value){
        body_[key] = std::forward<T>(value);
    }
    void setBody(QJsonObject&& body);
    void setBody(const QJsonObject& body);

    static Request fromJson(QByteArray json);
private:
    QString path_;
    QJsonObject body_;
};

#endif // REQUEST_H
