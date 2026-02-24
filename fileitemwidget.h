#ifndef FILESLISTWIDGET_H
#define FILESLISTWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMouseEvent>

class ClickableLabel : public QLabel {
    Q_OBJECT
public:
    explicit ClickableLabel(QWidget* parent=nullptr):QLabel(parent){}
signals:
    void clicked();
protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(); }
};

class FileItemWidget : public QWidget {
    Q_OBJECT
public:
    FileItemWidget(const QString& fileName, bool isLoaded, bool isNew, QWidget* parent = nullptr);

    void setLoaded(bool loaded);
    void setProgress(int bytesReceived, int totalBytes);

    void setContentId(int id){ content_id_=id; }
    int getContentId() const { return content_id_; }

    QString getFileName() const { return full_filename_; }
    void setFullFileName(QString name){ full_filename_=name; }

signals:
    void downloadClicked();
    void cancelClicked();
    void openClicked();

private slots:
    void onDownloadClicked();
    void onCancelClicked();
    void onOpenClicked();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool isImageFile(const QString& name);
    void showImagePreview();
    void showPlaceholder();

private:
    int content_id_=-1;
    bool isNew_;
    bool is_image_;

    QString full_filename_;

    // COMMON
    QLabel* status_label_;
    QLabel* progress_label_;
    QPushButton* command_button_;

    // FILE MODE (старый вид)
    QHBoxLayout* file_layout_;
    QLabel* file_name_label_;

    // IMAGE MODE
    QVBoxLayout* image_layout_;
    QLabel* image_label_;
};


class FilesListWidget : public QWidget {
    Q_OBJECT
public:
    explicit FilesListWidget(QWidget* parent = nullptr);

    void setFiles(const QList<QString>& files,bool isNewFiles,int messageId,int chatId);

    void setMessageId(int id){ message_id_=id; }
    void setChatId(int id){ chat_id_=id; }

signals:
    void fileDownloadRequested(int messageId,int fileIndex,QString filename);
    void fileSendCancel(int messageId,int fileIndex);

public slots:
    void SetLoadProgress(int messageId,int fileIndex,qint64 uploaded,qint64 total);
    void SetCompleteLoad(int messageId,int fileIndex,int newContentId);

    void fileDownloadProgressSlot(int messageId,int fileIndex,qint64 bytesReceived,qint64 total);
    void fileDownloadCompletedSlot(int messageId,int fileIndex);

private:
    QVBoxLayout* layout_;
    QList<FileItemWidget*> file_items_;
    int message_id_;
    int chat_id_;
};

#endif
