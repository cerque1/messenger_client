#include "fileitemwidget.h"
#include <QFile>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>

static const int IMG_SIZE = 400;

bool FileItemWidget::isImageFile(const QString& name){
    QString n=name.toLower();
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    return n.endsWith(".png")||n.endsWith(".jpg")||n.endsWith(".jpeg")
           ||n.endsWith(".bmp")||n.endsWith(".gif")||n.endsWith(".webp");
}

FileItemWidget::FileItemWidget(const QString& fileName,bool isLoaded,bool isNew,QWidget* parent)
    : QWidget(parent),isNew_(isNew)
{
    full_filename_=fileName;
    is_image_=isImageFile(fileName);
    
    // Устанавливаем прозрачный фон с бордером для всего виджета файла
    this->setStyleSheet(
        "QWidget {"
        "    background-color: transparent;"
        "    border: 1px solid #23262b;"
        "    border-radius: 10px;"
        "    padding: 4px;"
        "}");

    status_label_=new QLabel;
    status_label_->setStyleSheet("color: #a1a1aa; font-size: 12px;");
    progress_label_=new QLabel;
    progress_label_->setStyleSheet("color: #a1a1aa; font-size: 11px;");
    progress_label_->hide();

    command_button_=new QPushButton(isNew?"Отменить":"Загрузить");
    command_button_->setFixedWidth(110);
    command_button_->setStyleSheet(
        "QPushButton {"
        "    background-color: #2563eb;"
        "    border: 1px solid #2563eb;"
        "    border-radius: 10px;"
        "    color: white;"
        "    font-weight: 600;"
        "    padding: 5px 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #3b82f6;"
        "    border: 1px solid #3b82f6;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1d4ed8;"
        "    border: 1px solid #1d4ed8;"
        "}");

    if(is_image_)
    {
        // ---------- IMAGE UI ----------
        image_layout_=new QVBoxLayout(this);
        image_layout_->setAlignment(Qt::AlignCenter);
        image_layout_->setContentsMargins(8, 8, 8, 8);
        // image_layout_->setSpacing(6);

        image_label_=new QLabel;
        image_label_->setMaximumSize(IMG_SIZE,IMG_SIZE);
        image_label_->setAlignment(Qt::AlignCenter);

        image_layout_->addWidget(status_label_,0,Qt::AlignCenter);
        image_layout_->addWidget(image_label_,0,Qt::AlignCenter);
        image_layout_->addWidget(progress_label_,0,Qt::AlignCenter);
        image_layout_->addWidget(command_button_,0,Qt::AlignCenter);

        image_label_->installEventFilter(this);
    }
    else
    {
        // ---------- OLD FILE UI ----------
        file_layout_=new QHBoxLayout(this);
        file_layout_->setContentsMargins(8, 8, 8, 8);

        file_name_label_=new QLabel(fileName.split("/").last());
        file_name_label_->setStyleSheet("color: #ffffff; font-size: 12px;");

        file_layout_->addWidget(status_label_);
        file_layout_->addWidget(file_name_label_);
        file_layout_->addStretch();
        file_layout_->addWidget(command_button_);
        file_layout_->addWidget(progress_label_);
    }

    if(isNew_)
        connect(command_button_,&QPushButton::clicked,this,&FileItemWidget::onCancelClicked);
    else
        connect(command_button_,&QPushButton::clicked,this,&FileItemWidget::onDownloadClicked);

    setLoaded(isLoaded);
}

// ---------- IMAGE HELPERS ----------

void FileItemWidget::showPlaceholder(){
    if(!is_image_) return;
    image_label_->setStyleSheet("background-color: #15171a; border: 1px solid #23262b; border-radius: 8px;");
    image_label_->setPixmap(QPixmap());
}

#include <QImageReader>

void FileItemWidget::showImagePreview(){
    QString path="temp/files/"+full_filename_;

    QImageReader reader(path);
    reader.setAutoTransform(true);

    QSize size = reader.size();
    size.scale(IMG_SIZE, IMG_SIZE, Qt::KeepAspectRatio);
    reader.setScaledSize(size);

    QImage img = reader.read();
    if(img.isNull()){
        showPlaceholder();
        return;
    }

    QPixmap pix = QPixmap::fromImage(img);
    image_label_->setStyleSheet("background:transparent;");
    image_label_->setPixmap(pix);
}


// ---------- STATE ----------

void FileItemWidget::setLoaded(bool loaded){
    progress_label_->hide();

    if(loaded){
        status_label_->hide();
        command_button_->hide();

        if(is_image_)
            showImagePreview();
        else{
            command_button_->setText("Открыть");
            command_button_->setStyleSheet(
                "QPushButton {"
                "    background-color: #2563eb;"
                "    border: 1px solid #2563eb;"
                "    border-radius: 10px;"
                "    color: white;"
                "    font-weight: 600;"
                "    padding: 5px 12px;"
                "}"
                "QPushButton:hover {"
                "    background-color: #3b82f6;"
                "    border: 1px solid #3b82f6;"
                "}"
                "QPushButton:pressed {"
                "    background-color: #1d4ed8;"
                "    border: 1px solid #1d4ed8;"
                "}");
            command_button_->show();
            disconnect(command_button_,&QPushButton::clicked, nullptr, nullptr);
            connect(command_button_,&QPushButton::clicked,this,&FileItemWidget::onOpenClicked,Qt::UniqueConnection);
        }
    }
    else{
        status_label_->show();
        status_label_->setText(isNew_?"В отправке":"Не загружен");

        command_button_->show();
        command_button_->setText(isNew_?"Отменить":"Загрузить");

        if(is_image_)
            showPlaceholder();
    }
}

// ---------- PROGRESS ----------

void FileItemWidget::setProgress(int rec,int total){
    if(total<=0) return;

    double p=(double)rec/total*100.0;
    progress_label_->setText(QString("%1% (%2/%3)").arg((int)p).arg(rec).arg(total));
    progress_label_->show();
    command_button_->hide();

    if(is_image_)
        showPlaceholder();
}

// ---------- ACTIONS ----------

void FileItemWidget::onOpenClicked(){
    QString path="temp/files/"+full_filename_;
    if(QFile::exists(path)){
        emit openClicked();
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FileItemWidget::onDownloadClicked(){ emit downloadClicked(); }
void FileItemWidget::onCancelClicked(){ emit cancelClicked(); }

// ---------- IMAGE CLICK ----------

bool FileItemWidget::eventFilter(QObject* obj,QEvent* ev){
    if(obj==image_label_ && ev->type()==QEvent::MouseButtonPress){
        onOpenClicked();
        return true;
    }
    return QWidget::eventFilter(obj,ev);
}



// ================= LIST =================

FilesListWidget::FilesListWidget(QWidget* parent):QWidget(parent),message_id_(0){
    this->setStyleSheet("QWidget { background-color: transparent; }");
    layout_=new QVBoxLayout(this);
    layout_->setContentsMargins(0,0,0,0);
}

void FilesListWidget::setFiles(const QList<QString>& files,bool isNew,int messageId,int chatId){
    message_id_=messageId;
    chat_id_=chatId;

    qDeleteAll(file_items_);
    file_items_.clear();

    QLayoutItem* child;
    while((child=layout_->takeAt(0))){
        delete child->widget();
        delete child;
    }

    for(int i=0;i<files.size();++i){
        QString name=files[i];
        bool loaded=QFile::exists("temp/files/"+name);

        auto* item=new FileItemWidget(name.split("/").last(),loaded,isNew,this);

        connect(item,&FileItemWidget::downloadClicked,[this,i]{
            emit fileDownloadRequested(message_id_,i,file_items_[i]->getFileName());
        });

        connect(item,&FileItemWidget::cancelClicked,[this,i]{
            emit fileSendCancel(message_id_,i);
        });

        file_items_.append(item);
        layout_->addWidget(item);
    }
}

void FilesListWidget::SetLoadProgress(int id,int idx,qint64 up,qint64 total){
    if(id!=message_id_) return;
    file_items_[idx]->setProgress(up,total);
}

void FilesListWidget::SetCompleteLoad(int id,int idx,int cid){
    if(id!=message_id_) return;

    QString name=QString("%1_%2_%3_%4")
                       .arg(cid).arg(message_id_).arg(chat_id_)
                       .arg(file_items_[idx]->getFileName());

    file_items_[idx]->setFullFileName(name);
    file_items_[idx]->setContentId(cid);
    file_items_[idx]->setLoaded(true);
}

void FilesListWidget::fileDownloadProgressSlot(int id,int idx,qint64 r,qint64 t){
    if(id!=message_id_) return;
    file_items_[idx]->setProgress(r,t);
}

void FilesListWidget::fileDownloadCompletedSlot(int id,int idx){
    if(id!=message_id_) return;
    file_items_[idx]->setLoaded(true);
    disconnect(file_items_[idx], &FileItemWidget::downloadClicked, nullptr, nullptr);
}
