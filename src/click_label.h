#ifndef CLICKLABEL_H
#define CLICKLABEL_H

#include <QLabel>

class ClickLabel : public QLabel {
  Q_OBJECT

 public:
  explicit ClickLabel(QWidget *parent = nullptr);
  void set_normal_pix(const QPixmap &pix);
  void set_hover_pix(const QPixmap &pix);
  void set_press_pix(const QPixmap &pix);

 signals:
  void clicked();

 protected:
  void mousePressEvent(QMouseEvent *ev) override;
  void mouseReleaseEvent(QMouseEvent *ev) override;
  void enterEvent(QEvent *event) override;
  void leaveEvent(QEvent *event) override;

 private:
  bool left_pressed_{false};
  bool right_pressed_{false};
  QPixmap normal_pix_;
  QPixmap hover_pix_;
  QPixmap press_pix_;
};

#endif  // CLICKLABEL_H
