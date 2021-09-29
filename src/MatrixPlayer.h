#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#include "MatrixAudioPlayer.h"
#include "MatrixVideoPlayer.h"
#include "muebtransmitter.h"

class MatrixPlayerListener;

class MatrixPlayer {
  class VideoListener : public MatrixVideoPlayerListener {
   public:
    VideoListener(MatrixPlayer& parent) : parent(parent){};
    void onStateChanged(MatrixVideoPlayer::eState) override;
    void onTimeChanged(double time) override;
    void onFrameChanged(const QImage& frame) override;
    void onTrackEnded() override;

   private:
    MatrixPlayer& parent;
  };

  class AudioListener : public MatrixAudioPlayerListener {
   public:
    AudioListener(MatrixPlayer& parent) : parent(parent){};
    void onStateChanged(MatrixAudioPlayer::eState) override;
    void onTrackEnded() override;

   private:
    MatrixPlayer& parent;
  };

 public:
  enum eState {
    EMPTY,
    STOPPED,
    PAUSED,
    PLAYING,
  };

  MatrixPlayer();
  ~MatrixPlayer();

  // --- Playback control --- //
  void play();
  void pause();
  void stop();

  template <class Rep, class Period>
  void setTime(std::chrono::duration<Rep, Period> time);

  void setVolume(float volume);

  // --- Get state --- //
  eState getState() const;
  std::chrono::microseconds getTime() const;
  std::chrono::microseconds getDuration() const;

  size_t width() const { return videoPlayer.width(); }
  size_t height() const { return videoPlayer.height(); }

  float getVolume() const;

  void addListener(MatrixPlayerListener*);
  void removeListener(MatrixPlayerListener*);

  // --- Input data --- //
  bool load(const std::string& filePath);
  void clear();

 private:
  void notifyListenersState(eState state);
  void notifyListenersTime(double time);
  void notifyListenersTrackEnd();
  void notifyListenersFrame(const QImage& frame);
  volatile bool videoEndedFlag;
  volatile bool audioEndedFlag;

  libmueb::MuebTransmitter& transmitter;
  MatrixVideoPlayer videoPlayer;
  VideoListener videoListener;
  MatrixAudioPlayer audioPlayer;
  AudioListener audioListener;
  bool hasAudio;

  std::thread synchronizerThread;
  void startSynchronizer();
  void stopSynchronizer();
  volatile bool runSynchronizer;
  mutable std::mutex subPlayerMutex;

  std::set<MatrixPlayerListener*> listeners;
};

template <class Rep, class Period>
void MatrixPlayer::setTime(std::chrono::duration<Rep, Period> time) {
  audioEndedFlag = videoEndedFlag = false;
  videoPlayer.play();
  audioPlayer.play();
  videoPlayer.setTime(time);
  audioPlayer.setTime(time);
}

class MatrixPlayerListener {
 public:
  virtual void onStateChanged(MatrixPlayer::eState) = 0;
  virtual void onTimeChanged(double time) = 0;
  virtual void onFrameChanged(const QImage& frame) = 0;
  virtual void onTrackEnded() = 0;
};
