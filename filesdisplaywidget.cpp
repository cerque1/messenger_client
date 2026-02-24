#include "FilesDisplayWidget.h"

FilesDisplayWidget::FilesDisplayWidget(QWidget *parent)
    : QWidget(parent) {
    layout_ = new QHBoxLayout(this);
    layout_->setContentsMargins(5, 5, 5, 5);
    layout_->setSpacing(6);
    setFixedHeight(36);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hide();

    setStyleSheet("FilesDisplayWidget { background: transparent; }");
}

FilesDisplayWidget::~FilesDisplayWidget() {
    clearFiles();
}

void FilesDisplayWidget::addFileName(const QString& filePath) {
    if (file_labels_.size() >= 10) return;

    QString fileName = QFileInfo(filePath).fileName();

    ClickableFileLabel* fileLabel = new ClickableFileLabel(this);
    fileLabel->setProperty("fileName", fileName);
    fileLabel->setProperty("fullName", fileName);
    fileLabel->setAlignment(Qt::AlignCenter);

    connect(fileLabel, &ClickableFileLabel::clicked, this, &FilesDisplayWidget::onFileLabelClicked);

    file_labels_.append(fileLabel);
    layout_->addWidget(fileLabel);
    recalculateLabelSizes();

    if (isHidden()) {
        show();
    }
}

void FilesDisplayWidget::clearFiles() {
    qDeleteAll(file_labels_);
    file_labels_.clear();

    while (QLayoutItem* item = layout_->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    hide();
}

void FilesDisplayWidget::recalculateLabelSizes() {
    if (file_labels_.isEmpty()) return;

    int available_width = width() - 30;
    int spacing_total = (file_labels_.size() - 1) * layout_->spacing();
    int label_width = (available_width - spacing_total) / file_labels_.size();

    label_width = qMax(45, label_width);
    int height = 26;

    QFontMetrics fm(font());

    for (QLabel* label : file_labels_) {
        QString fullName = label->property("fullName").toString();
        QString displayText;

        if (file_labels_.size() == 1) {
            displayText = fullName;
        } else {
            int max_chars = qMin(20, (label_width - 10) / 8);
            displayText = fullName.left(max_chars);
            if (displayText.isEmpty()) {
                displayText = fullName.left(1);
            }
        }

        label->setText(displayText);
        label->setFixedSize(label_width, height);

        label->setStyleSheet(
            QString("QLabel { "
                    "background-color: #15171a; "
                    "border: 1px solid #23262b; "
                    "color: #ffffff; "
                    "border-radius: 10px; "
                    "padding: 4px 8px; "
                    "font-weight: 500; "
                    "font-size: 12px; "
                    "qproperty-alignment: AlignCenter; "
                    "} "
                    "QLabel:hover { "
                    "background-color: #181b1f; "
                    "border: 1px solid #2f3339; "
                    "}")
            );
    }

    updateGeometry();
    update();
}

void FilesDisplayWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    recalculateLabelSizes();
}

void FilesDisplayWidget::onFileLabelClicked() {
    ClickableFileLabel* label = qobject_cast<ClickableFileLabel*>(sender());
    if (!label) return;

    QString fileName = label->property("fileName").toString();
    file_labels_.removeOne(label);
    layout_->removeWidget(label);
    label->deleteLater();

    recalculateLabelSizes();

    if (file_labels_.isEmpty()) {
        hide();
    }

    emit removeFile(fileName);
}
