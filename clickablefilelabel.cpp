#include "ClickableFileLabel.h"

ClickableFileLabel::ClickableFileLabel(QWidget* parent)
    : QLabel(parent) {
    setCursor(Qt::PointingHandCursor);
}

void ClickableFileLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}
