#pragma once

#include <atomic>
#include <chrono>
#include <fmod.hpp>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

class MatrixAudioPlayerListener;

class MatrixAudioPlayer {
 public:
  enum eState {
    EMPTY,
    STOPPED,
    PAUSED,
    PLAYING,
  };

 public:
  MatrixAudioPlayer();
  ~MatrixAudioPlayer();

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

  float getVolume() const;

  void addListener(MatrixAudioPlayerListener*);
  void removeListener(MatrixAudioPlayerListener*);

  // --- Input data --- //
  bool load(const void* data, size_t size);
  void clear();

 private:
  // administration stuff
  std::set<MatrixAudioPlayerListener*> listeners;
  void notifyListenersTrackEnded();
  std::thread trackEndedPollThread;
  mutable std::mutex mtx;
  std::atomic_bool runPollThread;

  // sound stuff
  struct Deleter {
    void operator()(void* ptr) { operator delete(ptr); }
  };

  std::unique_ptr<void, Deleter> data;
  size_t size;
  std::atomic<eState> state;

  float volume = 1.0f;

  // Fmod stuff
  FMOD::System* system = nullptr;
  FMOD::Sound* sound = nullptr;
  FMOD::Channel* channel = nullptr;
  void releaseFmodObjects();
};

template <class Rep, class Period>
void MatrixAudioPlayer::setTime(std::chrono::duration<Rep, Period> time) {
  std::lock_guard<std::mutex> lk(mtx);
  if (state == PLAYING || state == PAUSED) {
    unsigned position =
        std::chrono::duration_cast<std::chrono::milliseconds>(time).count();
    channel->setPosition(position, FMOD_TIMEUNIT_MS);
  }
}

class MatrixAudioPlayerListener {
 public:
  virtual void onStateChanged(MatrixAudioPlayer::eState) = 0;
  virtual void onTrackEnded() = 0;
};
