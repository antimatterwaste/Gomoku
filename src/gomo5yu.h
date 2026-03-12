#ifndef TESTCHESS_H
#define TESTCHESS_H

#include <QMediaPlayer>
#include <QWidget>

namespace Ui {
class Form;
}
class QMediaPlayer;

struct UiDesign {
  QSizeF boardSizeWithMargin{900, 900};
  double margin{60};
  const double threadWidth{0.5};
  int horizontalThreadCount{19};
  int verticalThreadCount{19};
  int alpha{255};
  QColor bgColor{110, 123, 108, alpha};
  QColor senteColor{40, 30, 20, qBound(0, alpha + 50, 255)};
  QColor goteColor{220, 217, 220, alpha};
  QColor threadColor{68, 68, 69, alpha};
  QColor buttonNormalBg{30, 30, 30, alpha};
  QColor buttonHoverBg{100, 100, 100, alpha};
  QColor buttonPressBg{0, 0, 0, alpha};
  QColor buttonNormalText{100, 100, 100, alpha};
  QColor buttonHoverText{255, 255, 255, alpha};
  QColor buttonPressText{buttonNormalText};
  QSizeF chessBoardSize() {
    return QSizeF{boardSizeWithMargin.width() - 2 * margin,
                  boardSizeWithMargin.height() - 2 * margin};
  }
  void update_alpha() {
    bgColor.setAlpha(alpha);
    senteColor.setAlpha(alpha);
    goteColor.setAlpha(alpha);
    threadColor.setAlpha(alpha);
    buttonNormalBg.setAlpha(alpha);
    buttonHoverBg.setAlpha(alpha);
    buttonPressBg.setAlpha(alpha);
    buttonNormalText.setAlpha(alpha);
    buttonHoverText.setAlpha(alpha);
    buttonPressText.setAlpha(alpha);
  }
  void update_size() { margin = 20. / 300 * boardSizeWithMargin.width(); }
};

enum SoundType {
  RESET_GAME,
  MAKE_MOVE,
  WIN,
  RETRACT,
};

class Gomo5yu : public QWidget {
  Q_OBJECT

 public:
  Gomo5yu(QWidget *parent = nullptr);
  ~Gomo5yu();

 protected:
  void wheelEvent(QWheelEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void paintEvent(QPaintEvent *event);
  void leaveEvent(QEvent *event);
  void timerEvent(QTimerEvent *event);

 private:
  void make_move(const QPointF &mouse_pos);
  QPoint move_pos(const QPointF &mouse_pos) const;
  double board_x_begin() const;
  double board_x_end() const;
  double board_y_begin() const;
  double board_y_end() const;
  double board_cell_width() const;
  double board_cell_height() const;
  double piece_diameter() const;
  QRectF piece_rect(const QPoint &piece_index) const;
  void get_button_pix(const QString &button_text, QPixmap &normal,
                      QPixmap &hover, QPixmap &press);
  void handle_click_close();
  void handle_click_reset();
  void handle_click_pin_window();
  QSizeF get_size_from_board_size(const QSizeF &board_size) const;
  QPointF get_board_pos_from_window_pos(const QPointF &window_pos) const;
  double get_board_y_offset() const;
  QRectF board_rect() const;
  QRectF bar_rect() const;
  int checkFiveInARow(const QList<QPoint> &move_list,
                      const QList<bool> &role_list);
  void update_button();
  QPixmap get_noise_pix(const QSize &size, int noise_width, int rgba_max);
  int random_int(int min, int max);
  QPixmap get_piece_pix(const QSize &size, bool is_black);
  QString get_random_emoticon();
  void init_emoticon_list();
  void play_sound(SoundType type);
  void handle_playback_error(QMediaPlayer::Error error);

 private:
  Ui::Form *ui;
  UiDesign design_;
  bool left_pressed_{false};
  bool right_pressed_{false};
  int move_count_{0};
  QList<QPoint> move_list_;
  QList<QPoint> win_move_list_;
  QList<bool> move_sente_or_gote_list_;
  QPoint cursor_global_pos_when_pressed_;
  QPoint pos_when_pressed_;
  QPoint cursor_pos_when_pressed_;
  QPoint hover_move_{-1, -1};
  int game_result_{-1};
  bool pin_window_{false};
  QPixmap noise_pix_;
  QStringList emoticon_list_;
  QString tip_;
  QMediaPlayer *media_player_{nullptr};
  int last_move_sound_index_{0};
};
#endif  // TESTCHESS_H
