#ifndef MATRIXPLAYERWINDOW_H
#define MATRIXPLAYERWINDOW_H

#include <QMainWindow>
#include <mutex>

#include "MatrixPlayer.h"

class QStringListModel;
class PlayListItem;
class QListWidgetItem;
class QGraphicsScene;

namespace Ui {
class MatrixPlayerWindow;
}

class MatrixPlayerWindow;

class PlayerListener : public MatrixPlayerListener {
 public:
  PlayerListener(MatrixPlayerWindow& parent);
  void onStateChanged(MatrixPlayer::eState) override;
  void onTimeChanged(double time) override;
  void onFrameChanged(const QImage& frame) override;
  void onTrackEnded() override;

 private:
  MatrixPlayerWindow& parent;
};

class MatrixPlayerWindow : public QMainWindow {
  Q_OBJECT
  friend class PlayerListener;

 public:
  explicit MatrixPlayerWindow(QWidget* parent = 0);
  ~MatrixPlayerWindow();

 private slots:
  void on_updateTimeIndicator();

  void on_buttonAddMedia_clicked();

  void on_buttonRemoveMedia_clicked();

  void on_buttonClearMedia_clicked();

  void on_buttonPlay_clicked();

  void on_buttonStop_clicked();

  void on_playlistView_itemDoubleClicked(QListWidgetItem* item);

  void on_buttonPrevTrack_clicked();

  void on_buttonNextTrack_clicked();

  void on_mediaTimeIndicator_sliderPressed();

  void on_mediaTimeIndicator_sliderReleased();

  void on_trackEnded();

  void on_volumeSlider_valueChanged(int value);

  void on_checkAutoplay_clicked(bool checked);

  void on_buttonInsertBreakpoint_clicked();

 private:
  bool loadCurrentMedia();
  bool seekPlaylist(intptr_t offset);
  QString secondsToTimestamp(int seconds);

  Ui::MatrixPlayerWindow* ui;
  MatrixPlayer matrixPlayer;
  std::recursive_mutex matrixPlayerMutex;
  PlayListItem* currentMedia;
  bool shouldUpdateTime;
  std::atomic_bool autoplay;

  QTimer* timer;

  QGraphicsScene* graphicsScene;
  PlayerListener playerListener;

  QImage currentFrame;
  std::mutex frameMutex;
  void displayFrame(const QImage& frame);
  std::atomic_bool newFrame;
};

#endif  // MATRIXPLAYERWINDOW_H
