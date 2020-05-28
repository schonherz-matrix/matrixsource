#pragma once

#include <QColor>
#include <QImage>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <thread>

class MatrixVideoPlayerListener;

class MatrixVideoPlayer {
 public:
  enum eState {
    EMPTY,
    STOPPED,
    PAUSED,
    PLAYING,
  };

  MatrixVideoPlayer();
  ~MatrixVideoPlayer();

  // --- Playback control --- //

  /// Start playing set media.
  void play();
  void pause();
  void stop();

  template <class Rep, class Period>
  void setTime(std::chrono::duration<Rep, Period> time);
  template <class Rep, class Period>
  void syncToExternalSource(std::chrono::duration<Rep, Period> externalTime);

  // --- Get state --- //
  eState getState() const;
  std::chrono::microseconds getTime() const;
  std::chrono::microseconds getDuration() const {
    return state == EMPTY ? std::chrono::microseconds(0)
                          : frameTime * (intptr_t)frames.size();
  }

  size_t width() const { return width_; }
  size_t height() const { return height_; }

  void addListener(MatrixVideoPlayerListener*);
  void removeListener(MatrixVideoPlayerListener*);

  // --- Input data --- //

  bool load(std::string filePath);
  bool load(const QImage* frames, size_t numFrames,
            std::chrono::microseconds(frameTime));
  bool debugLoad(size_t numFrames);
  void debugSetFrameTime(double timeSec);
  void clear();

  // --- Present frame to daemon --- //
  std::function<void(const QImage&)> PresentFrame;

 private:
  void displayThreadFunc();

  void notifyListenersState(eState state);
  void notifyListenersTime(double time);
  void notifyListenersTrackEnd();
  void notifyListenersFrame(const QImage& frame);

 private:
  size_t currentFrame;  // tells which frame is currently being displayed
  std::chrono::microseconds
      targetTimeDelta;  // how much time is stream off from external time source
  std::thread displayThread;
  std::mutex mtx;
  std::condition_variable cv;
  std::queue<std::function<bool(std::chrono::microseconds)>> controlTaskQueue;

  std::atomic<eState> state;  // current state of the player

  std::vector<QImage> frames;  // buffer containing all the frames
  std::chrono::microseconds
      frameTime;  // how much time there's between 2 frames
  size_t width_ = 0, height_ = 0;

  std::set<MatrixVideoPlayerListener*> listeners;
};

template <class Rep, class Period>
void MatrixVideoPlayer::setTime(std::chrono::duration<Rep, Period> time) {
  if (state == PLAYING || state == PAUSED) {
    // acquire the mutex, then put the stub into the queue
    std::lock_guard<std::mutex> lk(mtx);
    controlTaskQueue.push([this, time](std::chrono::microseconds frameElapsed) {
      // compute required frame index and align playtime with next frame
      std::chrono::microseconds timeDesired =
          std::chrono::duration_cast<std::chrono::microseconds>(time);
      size_t numFrames = size_t(timeDesired.count() / frameTime.count());
      std::chrono::microseconds timeOvershoot =
          timeDesired - numFrames * frameTime;

      size_t frameDesired = numFrames;

      if (frameDesired < frames.size()) {
        std::this_thread::sleep_for(frameTime - timeOvershoot);
        currentFrame = frameDesired;
      }

      return false;  // interrupt frame and start over
    });
    cv.notify_all();
  }
}

template <class Rep, class Period>
void MatrixVideoPlayer::syncToExternalSource(
    std::chrono::duration<Rep, Period> externalTime) {
  if (state == PLAYING || state == PAUSED) {
    // acquire the mutex, then put the stub into the queue
    std::lock_guard<std::mutex> lk(mtx);
    controlTaskQueue.push(
        [this, externalTime](std::chrono::microseconds frameElapsed) {
          // compute difference from external time
          std::chrono::microseconds currentTime;
          currentTime = currentFrame * frameTime + frameElapsed;

          targetTimeDelta = externalTime - currentTime;

          return true;  // continue frame
        });
    cv.notify_all();
  }
}

class MatrixVideoPlayerListener {
 public:
  virtual void onStateChanged(MatrixVideoPlayer::eState) = 0;
  virtual void onTimeChanged(double time) = 0;
  virtual void onFrameChanged(const QImage& frame) = 0;
  virtual void onTrackEnded() = 0;
};
