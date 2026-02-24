#ifndef CHATHEADER_H
#define CHATHEADER_H

#include <QWidget>
#include <QMouseEvent>

namespace Ui {
class ChatHeader;
}

class ChatHeader : public QWidget
{
    Q_OBJECT

public:
    explicit ChatHeader(int chat_id, QString chat_name, QString last_time, QWidget *parent = nullptr);
    ~ChatHeader();

    void SetChatName(QString name);
    void SetChatTime(QString time);

signals:
    void clicked(int chat_id, QString chat_name);
    void startCallClicked(int chat_id, QString chat_name);

private slots:
    void startCallSlot();

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button() == Qt::LeftButton) {
            emit clicked(chat_id_, chat_name_);
        }
        QWidget::mousePressEvent(event);
    }

private:
    Ui::ChatHeader *ui;
    int chat_id_;
    QString chat_name_;
    QString last_time_;
};

#endif // CHATHEADER_H
