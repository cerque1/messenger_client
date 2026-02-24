#include "updater.h"

#include <QThread>

Updater::Updater(QObject *parent)
    : QObject{parent}
{
    UpdateWorker* worker = new UpdateWorker();
    QThread* thread = new QThread(this);
    worker->moveToThread(thread);

    connect(thread, SIGNAL(started()), worker, SLOT(process()));
    connect(this, SIGNAL(stop()), worker, SLOT(stop()));
    connect(this, SIGNAL(stop()), thread, SLOT(quit()));
    connect(worker, SIGNAL(finished()), worker, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));
    connect(worker, SIGNAL(Update(QList<entities::Chat>, QDateTime)), this, SIGNAL(UpdateUI(QList<entities::Chat>, QDateTime)));

    thread->start();
}

Updater::~Updater(){
    emit stop();
}
