#include "messageupdater.h"

#include <QThread>

MessageUpdater::MessageUpdater(QObject *parent)
    : QObject{parent}
{
    UpdateMessageWorker* worker = new UpdateMessageWorker();
    QThread* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(this, SIGNAL(stop()), worker, SLOT(stop()));
    connect(this, SIGNAL(stop()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(Update(entities::MessagesToChat)), this, SIGNAL(UpdateMessagesUi(entities::MessagesToChat)));
    connect(this, SIGNAL(start_timer()), worker, SLOT(process()));
    connect(this, SIGNAL(pause()), worker, SLOT(pause()));
    connect(this, SIGNAL(setChatId(int)), worker, SLOT(setChatId(int)));

    thread->start();
}

MessageUpdater::~MessageUpdater(){
    emit stop();
}
