#ifndef FILESDISPLAYWIDGET_H
#define FILESDISPLAYWIDGET_H

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QDebug>

#include "clickablefilelabel.h"

class FilesDisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit FilesDisplayWidget(QWidget *parent = nullptr);
    ~FilesDisplayWidget();

    void addFileName(const QString& filePath);
    void clearFiles();
    bool isEmpty() const { return file_labels_.isEmpty(); }

signals:
    void removeFile(const QString& fileName);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onFileLabelClicked();

private:
    void recalculateLabelSizes();

    QHBoxLayout* layout_;
    QList<ClickableFileLabel*> file_labels_;
};

#endif // FILESDISPLAYWIDGET_H
