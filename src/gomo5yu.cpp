#include "gomo5yu.h"
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <random>
#include <thread>
#include "click_label.h"
#include "ui_gomo5yu.h"

static thread_local std::mt19937_64 rng_engine([]() {
  std::random_device rd;
  auto seed = rd() ^
              static_cast<uint64_t>(
                  std::chrono::steady_clock::now().time_since_epoch().count()) ^
              (static_cast<uint64_t>(
                   std::hash<std::thread::id>()(std::this_thread::get_id()))
               << 1);
  return std::mt19937_64(seed);
}());

Gomo5yu::Gomo5yu(QWidget *parent) : QWidget(parent), ui(new Ui::Form) {
  ui->setupUi(this);
  ui->toolButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
  ui->toolButton->setPopupMode(QToolButton::InstantPopup);
  ui->toolButton->setStyleSheet(
      QString(R"(
        QToolButton {
          font-size: %1px;
          color: %2;
          padding: 0px;
          border: none;
          background-color: %3;
        }
        QToolButton::hover{
          color: %4;
          background-color:%5;
        }
        QToolButton::menu-indicator {
          image: none;
        }
      )")
          .arg(qBound(0,
                      static_cast<int>(
                          get_size_from_board_size(design_.boardSizeWithMargin)
                              .height() *
                          0.05),
                      15))
          .arg(design_.buttonNormalText.name())
          .arg(design_.buttonNormalBg.name())
          .arg(design_.buttonHoverText.name())
          .arg(design_.buttonHoverBg.name()));
  QMenu *menu = new QMenu(this);
  menu->setStyleSheet(
      QString(
          R"(QMenu{background-color:%1; color:%2} QMenu::item::selected{color:%3})")
          .arg(design_.bgColor.name())
          .arg(design_.buttonNormalText.name())
          .arg(design_.buttonHoverText.name()));
  QList<int> grid_list{3, 5, 7, 9, 15, 19, 21, 25};
  for (int i = 0; i < grid_list.size(); ++i) {
    QAction *action = new QAction(menu);
    action->setText(QString("%1 * %1").arg(grid_list.at(i)));
    action->setData(grid_list.at(i));
    menu->addAction(action);
  }
  QObject::connect(menu, &QMenu::triggered, this, [&](QAction *action) {
    if (!action) {
      return;
    }
    bool ok;
    int thread_count = action->data().toInt(&ok);
    if (!ok) {
      return;
    }
    design_.horizontalThreadCount = design_.verticalThreadCount = thread_count;
    handle_click_reset();
    update();
  });
  ui->toolButton->setMenu(menu);

  ui->top_widget->setCursor(Qt::OpenHandCursor);
  for (auto button : findChildren<ClickLabel *>()) {
    button->setCursor(Qt::PointingHandCursor);
  }
  media_player_ = new QMediaPlayer(this);
  connect(media_player_,
          QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this,
          &Gomo5yu::handle_playback_error);
  init_emoticon_list();
  setWindowFlags(Qt::FramelessWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  QSize ideal_size =
      get_size_from_board_size(design_.boardSizeWithMargin).toSize();
  setFixedSize(ideal_size);
  noise_pix_ = get_noise_pix(size(), 1, 40);
  QSize desk_size = qApp->desktop()->size();
  move((desk_size.width() - width()) / 2, (desk_size.height() - height()) / 2);
  update_button();
  connect(ui->close_label, &ClickLabel::clicked, this,
          &Gomo5yu::handle_click_close);
  connect(ui->reset_label, &ClickLabel::clicked, this,
          &Gomo5yu::handle_click_reset);
  startTimer(16);
}

Gomo5yu::~Gomo5yu() { delete ui; }

void Gomo5yu::wheelEvent(QWheelEvent *event) {
  if (event->delta() == 0) {
    return;
  }
  do {
    if (!(event->modifiers() & Qt::ControlModifier)) {
      break;
    }
    int diff = event->delta() > 0 ? 10 : -100;
    design_.alpha = qBound(30, design_.alpha + diff, 255);
    design_.update_alpha();
    update_button();
    update();
    return;
  } while (0);
  double ratio = event->delta() > 0 ? 1.1 : 0.9;
  QSizeF board_target_size = design_.boardSizeWithMargin * ratio;
  QSizeF current_window_size =
      get_size_from_board_size(design_.boardSizeWithMargin);
  QSizeF target_size = current_window_size * ratio;
  setFixedSize(target_size.toSize());
  if (noise_pix_.size() != size()) {
    noise_pix_ = get_noise_pix(size(), 1, 40);
  }
  move(static_cast<int>(
           x() - (target_size.width() - current_window_size.width()) / 2),
       static_cast<int>(
           y() - (target_size.height() - current_window_size.height()) / 2));
  design_.boardSizeWithMargin = board_target_size;
  design_.update_size();
  update_button();
  update();
}

void Gomo5yu::mouseReleaseEvent(QMouseEvent *event) {
  if (!left_pressed_ && event->button() == Qt::LeftButton) {
    return;
  }
  if (!right_pressed_ && event->button() == Qt::RightButton) {
    return;
  }
  if (game_result_ < 0 && Qt::LeftButton == event->button()) {
    make_move(event->pos());
  }
  if (Qt::RightButton == event->button() && !move_list_.isEmpty()) {
    move_list_.pop_back();
    move_sente_or_gote_list_.pop_back();
    --move_count_;
    game_result_ = checkFiveInARow(move_list_, move_sente_or_gote_list_);
    update();
    play_sound(RETRACT);
  }
}

void Gomo5yu::mousePressEvent(QMouseEvent *event) {
  Q_UNUSED(event)
  if (event->button() == Qt::LeftButton) {
    cursor_global_pos_when_pressed_ = event->globalPos();
    pos_when_pressed_ = pos();
    cursor_pos_when_pressed_ = event->pos();
    left_pressed_ = true;
  }
  if (event->button() == Qt::RightButton) {
    right_pressed_ = true;
  }
}

void Gomo5yu::mouseMoveEvent(QMouseEvent *event) {
  Q_UNUSED(event)
  QPoint cursor_diff = event->globalPos() - cursor_global_pos_when_pressed_;
  if (event->buttons() != Qt::LeftButton ||
      !ui->top_widget->geometry().contains(cursor_pos_when_pressed_)) {
    return;
  }
  left_pressed_ = false;
  right_pressed_ = false;
  move(pos_when_pressed_ + cursor_diff);
}

void Gomo5yu::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event)
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  painter.fillRect(rect(), design_.bgColor);
  painter.fillRect(
      QRect(0, 0, width(), static_cast<int>(get_board_y_offset()) + 5),
      QColor(30, 30, 30, design_.alpha));

  {
    QPen pen;
    pen.setWidthF(design_.threadWidth);
    pen.setColor(design_.threadColor);
    painter.setPen(pen);
    QPointF p1{board_x_begin(), board_y_begin()};
    QPointF p2{board_x_end(), board_y_begin()};
    for (int i = 0; i < design_.horizontalThreadCount; ++i) {
      painter.drawLine(p1, p2);
      p1.setY(p1.y() + board_cell_height());
      p2.setY(p2.y() + board_cell_height());
    }
    p1 = QPointF{board_x_begin(), board_y_begin()};
    p2 = QPointF{board_x_begin(), board_y_end()};
    for (int i = 0; i < design_.verticalThreadCount; ++i) {
      painter.drawLine(p1, p2);
      p1.setX(p1.x() + board_cell_width());
      p2.setX(p2.x() + board_cell_width());
    }
  }

  {
    painter.setPen(Qt::NoPen);
    for (int i = 0; i < move_list_.size(); ++i) {
      bool is_gote = i % 2 == 0;
      QPixmap piece_pix =
          get_piece_pix(piece_rect(move_list_.at(i)).size().toSize(), is_gote);
      int alpha = design_.alpha;
      if (game_result_ >= 0 && !win_move_list_.contains(move_list_.at(i))) {
        alpha = qBound(5, alpha - 200, 255);
      }
      painter.setOpacity(static_cast<double>(alpha) / 255);
      painter.drawPixmap(piece_rect(move_list_.at(i)).topLeft(), piece_pix);
      painter.setOpacity(1);
    }
  }

  if (design_.horizontalThreadCount % 2 && design_.verticalThreadCount % 2) {
    QList<QPoint> mark_points;
    mark_points << QPoint(design_.horizontalThreadCount / 2,
                          design_.verticalThreadCount / 2)
                << QPoint(3, 3) << QPoint(design_.horizontalThreadCount - 4, 3)
                << QPoint(3, design_.verticalThreadCount - 4)
                << QPoint(design_.horizontalThreadCount - 4,
                          design_.verticalThreadCount - 4);
    for (int i = 0; i < mark_points.size(); ++i) {
      if (!move_list_.contains(mark_points.at(i))) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(design_.threadColor);
        double d = 2. / 16. * board_cell_width();
        if (d < 2) {
          d = 2;
        }
        painter.drawEllipse(piece_rect(mark_points.at(i)).center(), d / 2,
                            d / 2);
      }
    }
  }

  {
    QPen pen;
    painter.setPen(move_count_ % 2 ? QColor(255, 255, 255, design_.alpha)
                                   : QColor(0, 0, 0, design_.alpha));
    painter.setBrush(Qt::NoBrush);
    if (game_result_ < 0 && hover_move_.x() >= 0 && hover_move_.y() >= 0) {
      painter.drawRect(piece_rect(hover_move_));
    }

    QFont font;
    font.setFamily("microsoft yahei");
    font.setPixelSize(15);
    painter.setFont(font);
    painter.setPen(QColor(88, 100, 100, design_.alpha));
    painter.drawText(QRect(20, height() - 30, width() - 20, 20), Qt::AlignLeft,
                     tip_);
  }

  painter.drawPixmap(0, 0, noise_pix_);
}

void Gomo5yu::leaveEvent(QEvent *event) {
  Q_UNUSED(event)
  left_pressed_ = false;
  right_pressed_ = false;
}

void Gomo5yu::timerEvent(QTimerEvent *event) {
  Q_UNUSED(event)
  QPoint cursor_pos = QCursor::pos() - pos();
  QPoint detect_move = move_pos(cursor_pos);
  if (move_list_.contains(detect_move) || detect_move.x() < 0 ||
      detect_move.y() < 0) {
    hover_move_ = QPoint{-1, -1};
  } else {
    hover_move_ = detect_move;
  }

  update();
}

void Gomo5yu::make_move(const QPointF &mouse_pos) {
  QPoint new_move = move_pos(mouse_pos);
  if (move_list_.contains(new_move) || new_move.x() < 0 || new_move.y() < 0) {
    return;
  }
  tip_ = get_random_emoticon();
  ++move_count_;
  move_list_ << move_pos(mouse_pos);
  move_sente_or_gote_list_ << (move_count_ % 2 != 0);
  game_result_ = checkFiveInARow(move_list_, move_sente_or_gote_list_);
  if (game_result_ >= 0) {
    play_sound(WIN);
  } else {
    play_sound(MAKE_MOVE);
  }
  update();
}

QPoint Gomo5yu::move_pos(const QPointF &mouse_pos) const {
  if (design_.horizontalThreadCount <= 0 || design_.verticalThreadCount <= 0) {
    return QPoint{-1, -1};
  }

  QPointF board_mouse_pos = get_board_pos_from_window_pos(mouse_pos);
  int horizontal_index = static_cast<int>(
      (board_mouse_pos.x() - design_.margin) / board_cell_width());
  int vertical_index = static_cast<int>((board_mouse_pos.y() - design_.margin) /
                                        board_cell_height());
  if (horizontal_index < 0 ||
      horizontal_index >= design_.horizontalThreadCount || vertical_index < 0 ||
      vertical_index >= design_.verticalThreadCount) {
    return QPoint(-1, -1);
  }

  QPoint top_left{horizontal_index, vertical_index};
  QPoint top_right{top_left.x() + 1, top_left.y()};
  QPoint bottom_left{top_left.x(), top_left.y() + 1};
  QPoint bottom_right{top_left.x() + 1, top_left.y() + 1};
  QList<QPoint> point_list{top_right, bottom_left, bottom_right};
  QPoint target = top_left;
  double min_distance =
      QLineF(QPointF(board_x_begin() + top_left.x() * board_cell_width(),
                     board_y_begin() - get_board_y_offset() +
                         top_left.y() * board_cell_height()),
             board_mouse_pos)
          .length();
  for (int i = 0; i < point_list.size(); ++i) {
    QLineF line{
        QPointF(board_x_begin() + point_list.at(i).x() * board_cell_width(),
                board_y_begin() - get_board_y_offset() +
                    point_list.at(i).y() * board_cell_height()),
        board_mouse_pos};
    if (line.length() < min_distance) {
      min_distance = line.length();
      target = point_list.at(i);
    }
  }

  if (target.x() < 0 || target.y() < 0 ||
      target.x() >= design_.horizontalThreadCount ||
      target.y() >= design_.verticalThreadCount) {
    return QPoint{-1, -1};
  }
  return target;
}

double Gomo5yu::board_x_begin() const {
  return design_.margin + design_.threadWidth / 2;
}

double Gomo5yu::board_x_end() const {
  return design_.boardSizeWithMargin.width() - design_.margin -
         design_.threadWidth / 2;
}

double Gomo5yu::board_y_begin() const {
  return design_.margin + design_.threadWidth / 2 + get_board_y_offset();
}

double Gomo5yu::board_y_end() const {
  return design_.boardSizeWithMargin.height() - design_.margin -
         design_.threadWidth / 2 + get_board_y_offset();
}

double Gomo5yu::board_cell_width() const {
  if (design_.horizontalThreadCount == 1) {
    return 0;
  }
  return (board_x_end() - board_x_begin()) /
         (design_.horizontalThreadCount - 1);
}

double Gomo5yu::board_cell_height() const {
  if (design_.verticalThreadCount == 1) {
    return 0;
  }
  return (board_y_end() - board_y_begin()) / (design_.verticalThreadCount - 1);
}

double Gomo5yu::piece_diameter() const {
  return qMin(board_cell_width(), board_cell_height()) * 1.;
}

QRectF Gomo5yu::piece_rect(const QPoint &piece_index) const {
  return QRectF{(board_x_begin() + piece_index.x() * board_cell_width()) -
                    piece_diameter() / 2,
                (board_y_begin() + piece_index.y() * board_cell_height()) -
                    piece_diameter() / 2,
                piece_diameter(), piece_diameter()};
}

void Gomo5yu::get_button_pix(const QString &button_text, QPixmap &normal,
                             QPixmap &hover, QPixmap &press) {
  QFont font;
  font.setFamily("microsoft yahei");
  font.setPixelSize(qBound(
      0,
      static_cast<int>(
          get_size_from_board_size(design_.boardSizeWithMargin).height() *
          0.05),
      15));
  QFontMetrics metrics{font};
  int h_spacing = 8;
  int v_spacing = 2;
  QSize pix_size{metrics.width(button_text) + h_spacing * 2,
                 metrics.height() + v_spacing * 2};
  QPixmap normal_pix{pix_size}, hover_pix{pix_size}, press_pix{pix_size};
  normal_pix.fill(Qt::transparent);
  hover_pix.fill(Qt::transparent);
  press_pix.fill(Qt::transparent);

  {
    QPainter painter(&normal_pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.fillRect(normal_pix.rect(), design_.buttonNormalBg);
    painter.setPen(design_.buttonNormalText);
    painter.drawText(normal_pix.rect(), Qt::AlignCenter, button_text);
  }
  {
    QPainter painter(&hover_pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.fillRect(hover_pix.rect(), design_.buttonHoverBg);
    painter.setPen(design_.buttonHoverText);
    painter.drawText(hover_pix.rect(), Qt::AlignCenter, button_text);
  }
  {
    QPainter painter(&press_pix);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(font);
    painter.fillRect(hover_pix.rect(), design_.buttonPressBg);
    painter.setPen(design_.buttonPressText);
    painter.drawText(hover_pix.rect(), Qt::AlignCenter, button_text);
  }
  normal = normal_pix;
  hover = hover_pix;
  press = press_pix;
}

void Gomo5yu::handle_click_close() { close(); }

void Gomo5yu::handle_click_reset() {
  if (!move_list_.isEmpty()) {
    play_sound(RESET_GAME);
    move_list_.clear();
    move_sente_or_gote_list_.clear();
    win_move_list_.clear();
    game_result_ = -1;
    move_count_ = 0;
    update();
  }
}

QSizeF Gomo5yu::get_size_from_board_size(const QSizeF &board_size) const {
  QLayout *layout = this->layout();
  QSizeF size = board_size;
  if (layout) {
    size.setHeight(size.height() + layout->contentsMargins().top() +
                   layout->contentsMargins().bottom());
  }
  size.setHeight(size.height() + ui->top_widget->height() + 15);
  return size;
}

QPointF Gomo5yu::get_board_pos_from_window_pos(
    const QPointF &window_pos) const {
  QPointF pos = window_pos;
  pos.setY(pos.y() - get_board_y_offset());
  return pos;
}

double Gomo5yu::get_board_y_offset() const {
  int offset_y = ui->top_widget->height();

  if (layout()) {
    offset_y += layout()->contentsMargins().top();
  }
  return static_cast<double>(offset_y);
}

QRectF Gomo5yu::board_rect() const {
  QRectF rect{0, get_board_y_offset(), design_.boardSizeWithMargin.width(),
              design_.boardSizeWithMargin.height()};
  return rect;
}

QRectF Gomo5yu::bar_rect() const {
  return QRectF{0, 0, static_cast<double>(width()), board_y_begin()};
}

int Gomo5yu::checkFiveInARow(const QList<QPoint> &move_list,
                             const QList<bool> &role_list) {
  if (move_list.size() < 5 || move_list.size() != role_list.size()) {
    return -1;
  }

  QPoint last_point = move_list.last();
  bool last_role = role_list.last();
  int target_x = last_point.x();
  int target_y = last_point.y();

  QList<QPoint> directions = {QPoint(1, 0), QPoint(0, 1), QPoint(1, 1),
                              QPoint(1, -1)};

  foreach(QPoint dir, directions) {
    win_move_list_.clear();
    win_move_list_ << last_point;
    int dx = dir.x();
    int dy = dir.y();
    int count = 1;

    for (int i = 1;; ++i) {
      int x = target_x + dx * i;
      int y = target_y + dy * i;
      int index = move_list.indexOf(QPoint(x, y));
      if (index != -1 && role_list[index] == last_role) {
        win_move_list_ << QPoint{x, y};
        ++count;
      } else {
        break;
      }
    }

    for (int i = 1;; ++i) {
      int x = target_x - dx * i;
      int y = target_y - dy * i;
      int index = move_list.indexOf(QPoint(x, y));
      if (index != -1 && role_list[index] == last_role) {
        win_move_list_ << QPoint{x, y};
        ++count;
      } else {
        break;
      }
    }

    if (count >= 5) {
      return last_role == true ? 1 : 0;
    }
  }

  return -1;
}

void Gomo5yu::update_button() {
  {
    QPixmap normal, hover, press;
    get_button_pix("＞﹏＜", normal, hover, press);
    ui->close_label->set_normal_pix(normal);
    ui->close_label->set_hover_pix(hover);
    ui->close_label->set_press_pix(press);
  }
  {
    QPixmap normal, hover, press;
    get_button_pix("Й", normal, hover, press);
    ui->reset_label->set_normal_pix(normal);
    ui->reset_label->set_hover_pix(hover);
    ui->reset_label->set_press_pix(press);
  }
}

QPixmap Gomo5yu::get_noise_pix(const QSize &size, int noise_width,
                               int rgba_max) {
  QImage image{size, QImage::Format_ARGB32};

  uchar *bits = image.bits();
  int width = image.width();
  int height = image.height();
  int bytesPerLine = image.bytesPerLine();

  int y = 0;
  while (y < height) {
    int x = 0;
    while (x < width) {
      int max = rgba_max;
      int r = random_int(0, max);
      int g = random_int(0, max);
      int b = random_int(0, max);
      int a = random_int(0, max);
      for (int i = 0; i < noise_width; ++i) {
        if (y + i >= height) {
          break;
        }
        uint *line = reinterpret_cast<uint *>(bits + (y + i) * bytesPerLine);
        for (int j = 0; j < noise_width; ++j) {
          if (x + j >= width) {
            break;
          }
          line[x + j] = qRgba(r, g, b, a);
        }
      }
      x += noise_width;
    }
    y += noise_width;
  }
  return QPixmap::fromImage(image);
}

int Gomo5yu::random_int(int min, int max) {
  std::uniform_int_distribution<int> dist(min, max);
  return dist(rng_engine);
}

QPixmap Gomo5yu::get_piece_pix(const QSize &size, bool is_sente) {
  if (size.width() <= 0 || size.height() <= 0) {
    return QPixmap();
  }

  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setRenderHint(QPainter::SmoothPixmapTransform);

  qreal padding = qMin(size.width(), size.height()) * 0.09;
  QRectF chessRect(padding, padding, size.width() - 2 * padding,
                   size.height() - 2 * padding);

  qreal offset = qMin(size.width(), size.height()) * 0.03;
  qreal shadowScale = 1.05;
  QRectF shadowRect = chessRect;
  shadowRect.translate(offset, offset);
  shadowRect.setWidth(chessRect.width() * shadowScale);
  shadowRect.setHeight(chessRect.height() * shadowScale);

  QRadialGradient shadowGradient(shadowRect.center(), shadowRect.width() / 2);
  if (is_sente) {
    shadowGradient.setColorAt(0.0, QColor(0, 0, 0, 0));
    shadowGradient.setColorAt(0.8, QColor(10, 10, 10, 40));
    shadowGradient.setColorAt(1.0, QColor(20, 20, 20, 60));
  } else {
    shadowGradient.setColorAt(0.0, QColor(0, 0, 0, 0));
    shadowGradient.setColorAt(0.8, QColor(50, 50, 50, 30));
    shadowGradient.setColorAt(1.0, QColor(80, 80, 80, 50));
  }

  painter.setBrush(shadowGradient);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(shadowRect);

  QRadialGradient gradient(chessRect.center(), chessRect.width() / 2);
  if (is_sente) {
    gradient.setColorAt(0.0, QColor(60, 60, 60));
    gradient.setColorAt(0.8, QColor(20, 20, 20));
    gradient.setColorAt(1.0, QColor(0, 0, 0));
  } else {
    gradient.setColorAt(0.0, QColor(255, 255, 255));
    gradient.setColorAt(0.8, QColor(240, 240, 240));
    gradient.setColorAt(1.0, QColor(220, 220, 220));
  }

  painter.setBrush(gradient);
  painter.setPen(Qt::NoPen);
  painter.drawEllipse(chessRect);

  QRectF highlightRect(chessRect.x() + chessRect.width() * 0.2,
                       chessRect.y() + chessRect.height() * 0.1,
                       chessRect.width() * 0.15, chessRect.height() * 0.15);

  QRadialGradient highlightGradient(highlightRect.center(),
                                    highlightRect.width() / 2);
  if (is_sente) {
    highlightGradient.setColorAt(0.0, QColor(100, 100, 100, 150));
    highlightGradient.setColorAt(1.0, QColor(0, 0, 0, 0));
  } else {
    highlightGradient.setColorAt(0.0, QColor(255, 255, 255, 200));
    highlightGradient.setColorAt(1.0, QColor(255, 255, 255, 0));
  }

  painter.setBrush(highlightGradient);
  painter.drawEllipse(highlightRect);

  painter.end();
  return pixmap;
}

QString Gomo5yu::get_random_emoticon() {
  if (emoticon_list_.isEmpty()) {
    return "";
  }
  int index = random_int(0, emoticon_list_.size() - 1);
  return emoticon_list_.at(index);
}

void Gomo5yu::init_emoticon_list() {
  emoticon_list_ << "/_ \\"
                 << "（；´д｀）ゞ"
                 << "＞﹏＜"
                 << "(っ °Д °;)っ"
                 << "(￣ ‘i ￣;)"
                 << "( *^-^)ρ(*╯^╰)"
                 << "＞︿＜"
                 << "o(￣┰￣*)ゞ"
                 << "(ノへ￣、)"
                 << "<(＿　＿)>"
                 << "≡(▔﹏▔)≡"
                 << "ヽ(*。>Д<)o゜"
                 << "::>_<::"
                 << "ε(┬┬﹏┬┬)3"
                 << "〒▽〒";
}

void Gomo5yu::play_sound(SoundType type) {
  if (!media_player_) {
    return;
  }
  QString filename = QCoreApplication::applicationDirPath() + "/sounds/";
  QString suffix = ".wav";
  if (RESET_GAME == type) {
    filename += "reset_1" + suffix;
  } else if (MAKE_MOVE == type) {
    int index = random_int(0, 3);
    if (index == last_move_sound_index_) {
      index = qBound(0, index + 1, 3);
    }
    last_move_sound_index_ = index;
    filename += QString("move_%1").arg(index) + suffix;
  } else {
    filename += "pass" + suffix;
  }
  if (!QFile::exists(filename)) {
    qDebug() << "sound file doesn't exist:" << filename;
    return;
  }
  const int voluem = 60;
  if (media_player_->volume() != voluem) {
    media_player_->setVolume(voluem);
  }
  media_player_->stop();
  media_player_->setMedia(QUrl::fromLocalFile(filename));
  media_player_->play();
}

void Gomo5yu::handle_playback_error(QMediaPlayer::Error error) {
  qDebug() << error;
  if (media_player_) {
    media_player_->stop();
  }
}
