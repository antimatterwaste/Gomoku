#include "click_label.h"
#include <QMouseEvent>

ClickLabel::ClickLabel(QWidget *parent) : QLabel(parent) {}

void ClickLabel::set_normal_pix(const QPixmap &pix) {
  normal_pix_ = pix;
  setPixmap(pix);
}

void ClickLabel::set_hover_pix(const QPixmap &pix) { hover_pix_ = pix; }

void ClickLabel::set_press_pix(const QPixmap &pix) { press_pix_ = pix; }

void ClickLabel::mousePressEvent(QMouseEvent *ev) {
  if (ev->button() == Qt::LeftButton) {
    left_pressed_ = true;
    setPixmap(press_pix_);
  }
  if (ev->button() == Qt::RightButton) {
    right_pressed_ = true;
  }
}

void ClickLabel::mouseReleaseEvent(QMouseEvent *ev) {
  if (Qt::LeftButton == ev->button() && left_pressed_) {
    left_pressed_ = false;
    setPixmap(normal_pix_);
    emit clicked();
  }
}

void ClickLabel::enterEvent(QEvent *event) {
  Q_UNUSED(event)
  setPixmap(hover_pix_);
}

void ClickLabel::leaveEvent(QEvent *event) {
  Q_UNUSED(event)
  left_pressed_ = false;
  right_pressed_ = false;
  setPixmap(normal_pix_);
}
