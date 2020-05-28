#include "MatrixAudioPlayer.h"

#include <algorithm>
#include <cstring>
#include <iostream>

using namespace std;
using namespace std::chrono;

MatrixAudioPlayer::MatrixAudioPlayer() {
  // initialize inside
  state = EMPTY;

  // initialize FMOD
  FMOD_RESULT result;
  unsigned version;

  result = FMOD::System_Create(&system);
  if (result != FMOD_OK) {
    cout << "Failed to create FMOD system.";
    return;
  }

  result = system->getVersion(&version);
  if (version < FMOD_VERSION) {
    cout << "FMOD version of header and lib don't match." << endl;
    system->release();
    system = nullptr;
    return;
  }

  result = system->init(32, FMOD_INIT_NORMAL, nullptr);
  if (result != FMOD_OK) {
    cout << "Failed to init FMOD system.";
    system->release();
    system = nullptr;
    return;
  }

  // start polling thread
  runPollThread = true;
  trackEndedPollThread = thread([this] {
    while (runPollThread) {
      mtx.lock();
      bool isChannelPlaying = true;
      if (state == PLAYING) {
        FMOD_RESULT result = channel->isPlaying(&isChannelPlaying);
        if (!isChannelPlaying) {
          state = STOPPED;
          notifyListenersTrackEnded();
        }
      }
      mtx.unlock();
      this_thread::sleep_for(milliseconds(50));
    }
  });
}

MatrixAudioPlayer::~MatrixAudioPlayer() {
  runPollThread = false;
  if (trackEndedPollThread.joinable()) {
    trackEndedPollThread.join();
  }

  clear();
  if (sound != nullptr) {
    sound->release();
    sound = nullptr;
  }
  if (system != nullptr) {
    system->release();
    system = nullptr;
  }
}

void MatrixAudioPlayer::play() {
  std::lock_guard<std::mutex> lk(mtx);

  if (state == STOPPED) {
    system->playSound(sound, 0, true, &channel);
    channel->setVolume(volume);
    channel->setPaused(false);
    state = PLAYING;
  } else if (state == PAUSED) {
    channel->setPaused(false);
    state = PLAYING;
  }
}

void MatrixAudioPlayer::pause() {
  std::lock_guard<std::mutex> lk(mtx);

  if (state == PLAYING) {
    channel->setPaused(true);
    state = PAUSED;
  }
}

void MatrixAudioPlayer::stop() {
  std::lock_guard<std::mutex> lk(mtx);
  if (state == PLAYING || state == PAUSED) {
    channel->stop();
    state = STOPPED;
  }
}

void MatrixAudioPlayer::setVolume(float volume) {
  std::lock_guard<std::mutex> lk(mtx);

  volume = std::max(0.0f, std::min(1.0f, volume));
  this->volume = volume;

  if (state != EMPTY) {
    channel->setVolume(volume);
  }
}

// --- Get state --- //
MatrixAudioPlayer::eState MatrixAudioPlayer::getState() const { return state; }

std::chrono::microseconds MatrixAudioPlayer::getTime() const {
  std::lock_guard<std::mutex> lk(mtx);

  if (state == PLAYING || state == PAUSED) {
    unsigned position = 0;
    channel->getPosition(&position, FMOD_TIMEUNIT_MS);
    return microseconds(position * 1000);
  } else {
    return microseconds(0);
  }
}

std::chrono::microseconds MatrixAudioPlayer::getDuration() const {
  std::lock_guard<std::mutex> lk(mtx);

  if (state != EMPTY) {
    unsigned length;
    sound->getLength(&length, FMOD_TIMEUNIT_MS);
    return microseconds(length * 1000);
  }

  return microseconds(0);
}

float MatrixAudioPlayer::getVolume() const { return volume; }

void MatrixAudioPlayer::addListener(MatrixAudioPlayerListener* listener) {
  lock_guard<mutex> lk(mtx);
  listeners.insert(listener);
}

void MatrixAudioPlayer::removeListener(MatrixAudioPlayerListener* listener) {
  lock_guard<mutex> lk(mtx);
  listeners.erase(listener);
}

void MatrixAudioPlayer::notifyListenersTrackEnded() {
  for (auto listener : listeners) {
    listener->onTrackEnded();
  }
}

// --- Input data --- //
bool MatrixAudioPlayer::load(const void* data, size_t size) {
  stop();
  std::lock_guard<std::mutex> lk(mtx);

  // make a copy of input data
  state = EMPTY;
  this->data.reset(operator new(size));
  this->size = size;
  memcpy(this->data.get(), data, size);

  // create an fmod sound
  if (!system) {
    return false;
  }
  FMOD_CREATESOUNDEXINFO soundInfo;
  memset(&soundInfo, 0, sizeof(soundInfo));
  soundInfo.cbsize = sizeof(soundInfo);
  soundInfo.length = this->size;
  FMOD_RESULT result =
      system->createSound(reinterpret_cast<const char*>(this->data.get()),
                          FMOD_OPENMEMORY | FMOD_LOOP_OFF, &soundInfo, &sound);
  if (result != FMOD_OK) {
    return false;
  }

  state = STOPPED;
  return true;
}

void MatrixAudioPlayer::clear() {
  stop();

  std::lock_guard<std::mutex> lk(mtx);

  if (sound != nullptr) {
    sound->release();
    sound = nullptr;
  }
  data.reset();
  state = EMPTY;
}
