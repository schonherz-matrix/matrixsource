#include "MatrixPlayerWindow.h"
#include "ui_MatrixPlayerWindow.h"

#include <QFileDialog>
#include <QGraphicsScene>
#include <QStringListModel>
#include <QTimer>
#include <chrono>
#include <cstdint>
#include <iostream>

using namespace std;
using namespace std::chrono;

class PlayListItem : public QListWidgetItem {
 public:
  PlayListItem(QListWidget* parent = nullptr, int type = Type)
      : QListWidgetItem(parent, type) {}

  PlayListItem(const QString& text, QListWidget* parent = 0, int type = Type)
      : QListWidgetItem(parent, type) {
    splitAbsolutePath(text, path, fileName);
    QListWidgetItem::setText(fileName);
  }

  PlayListItem(const QIcon& icon, const QString& text,
               QListWidget* parent = nullptr, int type = Type)
      : QListWidgetItem(icon, "", parent, type) {
    splitAbsolutePath(text, path, fileName);
    QListWidgetItem::setText(fileName);
  }

  virtual void setText(const QString& text) {
    splitAbsolutePath(text, path, fileName);
    QListWidgetItem::setText(fileName);
  }
  virtual QString text() const { return path + fileName; }

  virtual bool isBreakpoint() const { return false; }

 protected:
  void splitAbsolutePath(const QString& fullName, QString& path,
                         QString& fileName) {
    intptr_t index = fullName.lastIndexOf(QDir::separator());
    if (index < 0) {
      path = "";
      fileName = fullName;
    } else if (index + 1 == fullName.size()) {
      path = fullName;
      fileName = "";
    } else {
      path = fullName.mid(0, index + 1);
      fileName = fullName.mid(index + 1);
    }
  }

  QString path;
  QString fileName;
};

class PlayListBreakpoint : public PlayListItem {
 public:
  PlayListBreakpoint(QListWidget* parent = nullptr, int type = Type)
      : PlayListItem(parent, type) {
    PlayListItem::setText(text());
  }

  void setText(const QString& text) override {
    // empty
  }

  QString text() const override { return "---------------"; }

  bool isBreakpoint() const override { return true; }
};

MatrixPlayerWindow::MatrixPlayerWindow(QWidget* parent)
    : QMainWindow(parent),
      playerListener(*this),
      ui(new Ui::MatrixPlayerWindow) {
  ui->setupUi(this);

  ui->playlistView->setDragEnabled(true);
  ui->playlistView->setDragDropMode(QAbstractItemView::InternalMove);

  currentMedia = nullptr;
  shouldUpdateTime = true;

  timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(on_updateTimeIndicator()));
  timer->start(200);  // time specified in ms

  graphicsScene = new QGraphicsScene;
  ui->frameView->setScene(graphicsScene);
  ui->frameView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->frameView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  matrixPlayer.addListener(&playerListener);

  ui->volumeSlider->setValue(matrixPlayer.getVolume() * 100);

  // Set button icons
  ui->buttonPrevTrack->setIcon(
      style()->standardIcon(QStyle::SP_MediaSkipBackward));
  ui->buttonStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
  ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  ui->buttonNextTrack->setIcon(
      style()->standardIcon(QStyle::SP_MediaSkipForward));

  ui->buttonPrevTrack->setText("");
  ui->buttonStop->setText("");
  ui->buttonPlay->setText("");
  ui->buttonNextTrack->setText("");
}

MatrixPlayerWindow::~MatrixPlayerWindow() {
  timer->stop();
  matrixPlayer.removeListener(&playerListener);
  delete ui;
}

void MatrixPlayerWindow::on_buttonAddMedia_clicked() {
  QStringList files =
      QFileDialog::getOpenFileNames(this, "Add media", QString(), "*.q4x");
  for (auto it = files.begin(); it != files.end(); ++it) {
    QString fileName = *it;
    ui->playlistView->insertItem(ui->playlistView->currentRow() + 1,
                                 new PlayListItem(fileName));
    ui->playlistView->setCurrentRow(ui->playlistView->currentRow() + 1);
  }
}

void MatrixPlayerWindow::on_buttonInsertBreakpoint_clicked() {
  ui->playlistView->insertItem(ui->playlistView->currentRow() + 1,
                               new PlayListBreakpoint());
}

void MatrixPlayerWindow::on_buttonRemoveMedia_clicked() {
  auto selectedMedia = ui->playlistView->selectedItems();
  for (auto it = selectedMedia.begin(); it != selectedMedia.end(); ++it) {
    PlayListItem* item = dynamic_cast<PlayListItem*>(*it);
    if (item == currentMedia) {
      lock_guard<recursive_mutex> lk(matrixPlayerMutex);

      matrixPlayer.clear();

      intptr_t index = ui->playlistView->row(currentMedia);
      delete currentMedia;
      intptr_t numMedia = ui->playlistView->count();

      if (numMedia > 0 && index >= 0) {
        intptr_t nextIndex;
        if (index < numMedia) {
          nextIndex = index;
        } else {
          nextIndex = numMedia - 1;  // should be == index - 1
        }
        currentMedia =
            dynamic_cast<PlayListItem*>(ui->playlistView->item(nextIndex));
      } else {
        currentMedia = nullptr;
      }
    } else {
      delete item;
    }
  }
}

void MatrixPlayerWindow::on_buttonClearMedia_clicked() {
  matrixPlayer.clear();
  ui->playlistView->clear();
  currentMedia = nullptr;
  ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void MatrixPlayerWindow::on_buttonPlay_clicked() {
  lock_guard<recursive_mutex> lk(matrixPlayerMutex);

  auto state = matrixPlayer.getState();
  if (state == MatrixPlayer::EMPTY) {
    if (!currentMedia) {
      auto selectedItems = ui->playlistView->selectedItems();

      // if there's no media, get it from selection
      if (selectedItems.size() > 0) {
        currentMedia = dynamic_cast<PlayListItem*>(*selectedItems.begin());
      }
      // if there's no selection, play the very first
      else {
        if (ui->playlistView->count() > 0) {
          currentMedia = dynamic_cast<PlayListItem*>(ui->playlistView->item(0));
        }
      }
    }

    // get current track and load it
    if (currentMedia) {
      QFont font = currentMedia->font();
      font.setBold(true);
      font.setItalic(true);
      currentMedia->setFont(font);
      // if there's a current media already set, load and play it
      if (loadCurrentMedia()) {
        matrixPlayer.play();
      }
    }
  } else if (state == MatrixPlayer::STOPPED) {
    matrixPlayer.play();
  } else if (state == MatrixPlayer::PAUSED) {
    matrixPlayer.play();
  } else if (state == MatrixPlayer::PLAYING) {
    matrixPlayer.pause();
  }

  if (matrixPlayer.getState() == MatrixPlayer::PLAYING) {
    ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
  } else {
    ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  }
}

void MatrixPlayerWindow::on_buttonStop_clicked() {
  lock_guard<recursive_mutex> lk(matrixPlayerMutex);
  matrixPlayer.stop();
  ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void MatrixPlayerWindow::on_updateTimeIndicator() {
  if (shouldUpdateTime) {
    lock_guard<recursive_mutex> lk(matrixPlayerMutex);

    long long timeUs = matrixPlayer.getTime().count();

    // set slider
    ui->mediaTimeIndicator->setValue(timeUs / 1000);

    // set textual indicators
    long long timeSec = timeUs / 1000000;
    long long durationSec = matrixPlayer.getDuration().count() / 1000000;
    long long remainingSec = durationSec - timeSec;
    QString timeText = secondsToTimestamp(timeSec);
    QString remainingText = secondsToTimestamp(-remainingSec);
    ui->labelTimeElapsed->setText(timeText);
    ui->labelTimeRemaining->setText(remainingText);
  }
  bool expected = true;
  if (newFrame.compare_exchange_strong(expected, false)) {
    lock_guard<mutex> lk(frameMutex);
    displayFrame(currentFrame);
  }
}

void MatrixPlayerWindow::on_playlistView_itemDoubleClicked(
    QListWidgetItem* item) {
  lock_guard<recursive_mutex> lk(matrixPlayerMutex);

  matrixPlayer.clear();
  if (currentMedia) {
    QFont font = currentMedia->font();
    font.setBold(false);
    font.setItalic(false);
    currentMedia->setFont(font);
  }
  currentMedia = dynamic_cast<PlayListItem*>(item);
  on_buttonPlay_clicked();
}

void MatrixPlayerWindow::on_buttonPrevTrack_clicked() { seekPlaylist(-1); }

void MatrixPlayerWindow::on_buttonNextTrack_clicked() { seekPlaylist(1); }

void MatrixPlayerWindow::on_trackEnded() {
  bool isBreakpointHit = seekPlaylist(1);
  if ((ui->playlistView->count() > 0 &&
       ui->playlistView->item(0) == currentMedia) ||
      !autoplay || isBreakpointHit) {
    ui->buttonPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  } else {
    on_buttonPlay_clicked();
  }
}

bool MatrixPlayerWindow::seekPlaylist(intptr_t offset) {
  if (currentMedia) {
    // get next media
    intptr_t index, numItems, nextIndex;
    PlayListItem* nextMedia = currentMedia;
    bool isBreakpointHit = false;
    do {
      index = ui->playlistView->row(nextMedia);
      numItems = ui->playlistView->count();
      index = max(intptr_t(0), index);
      numItems = max(intptr_t(1), numItems);
      nextIndex = (index + numItems + offset) % numItems;
      nextMedia =
          dynamic_cast<PlayListItem*>(ui->playlistView->item(nextIndex));
      if (nextMedia->isBreakpoint()) {
        isBreakpointHit = true;
      }
    } while (nextMedia->isBreakpoint() && nextIndex != 0);

    lock_guard<recursive_mutex> lk(matrixPlayerMutex);

    MatrixPlayer::eState prevState = matrixPlayer.getState();
    matrixPlayer.clear();

    auto font = currentMedia->font();
    font.setBold(false);
    font.setItalic(false);
    currentMedia->setFont(font);
    font.setBold(true);
    font.setItalic(true);
    nextMedia->setFont(font);

    currentMedia = nextMedia;
    if (loadCurrentMedia() && prevState == MatrixPlayer::PLAYING) {
      matrixPlayer.play();
    }

    return isBreakpointHit;
  }
}

void MatrixPlayerWindow::on_mediaTimeIndicator_sliderPressed() {
  shouldUpdateTime = false;
}

void MatrixPlayerWindow::on_mediaTimeIndicator_sliderReleased() {
  lock_guard<recursive_mutex> lk(matrixPlayerMutex);
  intptr_t seekMs = ui->mediaTimeIndicator->value();
  matrixPlayer.setTime(milliseconds(seekMs));
  shouldUpdateTime = true;
}

// if there's a current media already set, load and play it
bool MatrixPlayerWindow::loadCurrentMedia() {
  if (currentMedia) {
    bool isLoaded = matrixPlayer.load(currentMedia->text().toStdString());
    if (isLoaded) {
      int durationUs = matrixPlayer.getDuration().count();
      QString durationText =
          "(" + secondsToTimestamp(durationUs / 1000000) + ") ";
      ui->labelTrackName->setText(
          durationText + dynamic_cast<PlayListItem*>(currentMedia)->text());
      ui->mediaTimeIndicator->setMaximum(durationUs / 1000 +
                                         1);  // time indicator in ms!!!
    } else {
      ui->labelTrackName->setText("media could not be loaded");
    }
    return isLoaded;
  }
  return false;
}

PlayerListener::PlayerListener(MatrixPlayerWindow& parent) : parent(parent) {}

void PlayerListener::onStateChanged(MatrixPlayer::eState) {
  // no action
}

void PlayerListener::onTrackEnded() {
  // ugly hack to have intptr_t registered
  static const int dummy = [] {
    qRegisterMetaType<intptr_t>("intptr_t");
    return 0;
  }();
  // play next or stop on first
  bool isInvoke =
      QMetaObject::invokeMethod(&parent, "on_trackEnded", Qt::QueuedConnection);
  cout << isInvoke << endl;
}

void PlayerListener::onTimeChanged(double time) {
  // no action
}

void PlayerListener::onFrameChanged(const QImage& frame) {
  lock_guard<mutex>(parent.frameMutex);
  parent.currentFrame = frame;
  parent.newFrame = true;
}

void MatrixPlayerWindow::displayFrame(const QImage& frame) {
  size_t width = matrixPlayer.width(), height = matrixPlayer.height();

  float scale;
  int frameWidth = ui->frameView->width();
  int frameHeight = ui->frameView->height();
  scale = std::min((float)frameWidth / (float)width,
                   (float)frameHeight / (float)height);
  ui->frameView->setTransform(QTransform(scale, 0, 0, 0, scale, 0, 0, 0, 1));
  graphicsScene->clear();
  QPixmap pixmap = QPixmap::fromImage(frame);
  graphicsScene->addPixmap(pixmap);
}

void MatrixPlayerWindow::on_volumeSlider_valueChanged(int value) {
  float volume = value / 100.0f;
  matrixPlayer.setVolume(volume);
}

QString MatrixPlayerWindow::secondsToTimestamp(int seconds) {
  bool isNegative = seconds < 0;
  seconds = abs(seconds);
  int secPart = seconds % 60;
  int minPart = seconds / 60 % 60;
  int hourPart = seconds / (60 * 60);
  QString str;
  str = isNegative ? "-" : "";
  if (hourPart > 0) str += QString::number(hourPart) + ":";
  if (minPart < 10) str += "0";
  str += QString::number(minPart) + ":";
  if (secPart < 10) str += "0";
  str += QString::number(secPart);

  return str;
}

void MatrixPlayerWindow::on_checkAutoplay_clicked(bool checked) {
  autoplay = checked;
}
