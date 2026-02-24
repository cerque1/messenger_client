#ifndef CHAT_H
#define CHAT_H

#include <QWidget>
#include <QMouseEvent>

#include "entities.h"

namespace Ui {
class chat;
}

class Chat : public QWidget
{
    Q_OBJECT

public:
    explicit Chat(const entities::Chat& chat, QWidget *parent = nullptr);
    ~Chat();

    void SetChatId(int chat_id) {
        this->chat_id = chat_id;
    }

    int GetChatId() {
        return chat_id;
    }

    QString GetName() const;
    QString GetLast() const;

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button() == Qt::LeftButton) {
            emit clicked();
        }
        QWidget::mousePressEvent(event);
    }

signals:
    void updateAction();
    void deleteAction();
    void clicked();

private slots:
    void showContextMenuSlot(const QPoint &pos);

private:
    int chat_id;
    Ui::chat *ui;
};

#endif // CHAT_H
