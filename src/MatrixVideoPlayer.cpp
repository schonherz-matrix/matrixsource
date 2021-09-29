#include "MatrixVideoPlayer.h"

#include <iostream>

#include "Q4XLoader.h"

using namespace std;
using namespace std::chrono;

////////////////////////////////////////////////////////////////////////////////
// Ctor

MatrixVideoPlayer::MatrixVideoPlayer() { state = EMPTY; }

MatrixVideoPlayer::~MatrixVideoPlayer() { stop(); }

////////////////////////////////////////////////////////////////////////////////
// Playback control

void MatrixVideoPlayer::play() {
  if (state == EMPTY) {
    return;
  } else if (state == STOPPED) {
    controlTaskQueue = decltype(controlTaskQueue)();  // clear...
    state = PLAYING;
    currentFrame = 0;
    targetTimeDelta = microseconds(0);

    // start display thread
    if (displayThread.joinable()) {
      displayThread.join();
    }
    displayThread = thread([this] { displayThreadFunc(); });
    notifyListenersState(state);
  } else if (state == PAUSED) {
    targetTimeDelta = microseconds(0);
    state = PLAYING;
    notifyListenersState(state);
  }
}

void MatrixVideoPlayer::pause() {
  targetTimeDelta = microseconds(0);
  state = PAUSED;
  notifyListenersState(state);
}

void MatrixVideoPlayer::stop() {
  // kill display thread
  state = STOPPED;
  notifyListenersState(state);
  if (displayThread.joinable()) {
    displayThread.join();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Get state

std::chrono::microseconds MatrixVideoPlayer::getTime() const {
  switch (+state) {
    case PAUSED:
    case PLAYING:
      return currentFrame * frameTime;
    case EMPTY:
    case STOPPED:
      return microseconds(0);
  }
}

auto MatrixVideoPlayer::getState() const -> eState { return state; }

////////////////////////////////////////////////////////////////////////////////
// Load stuff
bool MatrixVideoPlayer::load(std::string filePath) {
  clear();

  Q4XLoader loader;
  frameTime = microseconds(33333);  // 30 FPS

  if (!loader.load(filePath)) {
    return false;
  }
  loader.resample(frameTime);
  frames = loader.getFrames();
  width_ = loader.width();
  height_ = loader.height();

  state = STOPPED;
  return true;
}

bool MatrixVideoPlayer::load(const QImage* frames, size_t numFrames,
                             std::chrono::microseconds(frameTime)) {
  if (numFrames > 0) {
    clear();
    width_ = frames[0].width();
    height_ = frames[0].height();

    for (size_t i = 0; i < numFrames; i++) {
      if (frames[i].width() != width_ || frames[i].height() != height_) {
        return false;
      }
    }

    this->frameTime = frameTime;
    this->frames.clear();
    this->frames.resize(numFrames);
    this->frames.assign(frames, frames + numFrames);

    state = STOPPED;

    return true;
  } else {
    return false;
  }
}

void MatrixVideoPlayer::clear() {
  stop();
  frames.clear();
  state = EMPTY;
  notifyListenersState(state);
}

////////////////////////////////////////////////////////////////////////////////
// Internal stuff

void MatrixVideoPlayer::displayThreadFunc() {
  microseconds compensation(0);
  microseconds waitTime;
  auto lastTime = high_resolution_clock::now();
  microseconds deltaCompensation(0);

  while (state == PAUSED || state == PLAYING) {
    notifyListenersTime((frameTime * currentFrame).count() / 1.0e6);
    waitTime = frameTime - compensation;
    waitTime -= deltaCompensation;

    cout << "wait time = " << waitTime.count() / 1000.0f << " ms" << endl;
    cout << "delta     = " << targetTimeDelta.count() / 1000.0f << " ms"
         << endl;
    cout << "delta comp= " << deltaCompensation.count() / 1000.0f << " ms"
         << endl;

    // lock that mutex lel
    unique_lock<mutex> lk(mtx);

    bool isExtraTask = cv.wait_for(
        lk, waitTime, [this] { return controlTaskQueue.size() > 0; });
    if (isExtraTask) {
      // calculate time of waiting until this task was received
      auto now = high_resolution_clock::now();
      microseconds elapsedPartial = duration_cast<microseconds>(now - lastTime);

      // perform that extra task
      // extra tasks can be:
      // - new media time
      // - new target delta
      // - super accurate query for media time
      // if the task returns false, interrupt this frame and immediately start
      // new
      auto task = std::move(controlTaskQueue.front());
      controlTaskQueue.pop();
      if (!task(elapsedPartial)) {
        compensation = microseconds(0);
        lastTime = high_resolution_clock::now();
        continue;
      } else {
        compensation += elapsedPartial;
        continue;
      }
    } else {
      compensation = microseconds(0);
      // TODO: display every second frame!
      // dummy display code
      if (state == PAUSED) {
        cout << "Displaying paused frame " << currentFrame << endl;
        if (PresentFrame) {
          PresentFrame(frames[currentFrame]);
        }
        notifyListenersFrame(frames[currentFrame]);
      } else {
        cout << "Displaying running frame " << currentFrame << endl;
        if (PresentFrame) {
          PresentFrame(frames[currentFrame]);
        }
        notifyListenersFrame(frames[currentFrame]);
        currentFrame++;
      }
    }

    lk.unlock();

    auto now = high_resolution_clock::now();
    auto elapsed = duration_cast<microseconds>(now - lastTime);
    cout << "Actual frametime: " << elapsed.count() / 1000.0 << " ms" << endl;
    lastTime = now;

    if (currentFrame == frames.size()) {
      state = STOPPED;
      notifyListenersTrackEnd();
      notifyListenersState(state);
    }

    // converge to external source
    long long deltaUs = targetTimeDelta.count();
    long long frameUs = (deltaUs < 0 ? -1 : 1) * frameTime.count();
    long long deltaCompensationUs;
    deltaUs *= 0.1;
    frameUs *= 0.1;
    if (abs(deltaUs) < abs(frameUs))
      deltaCompensationUs = deltaUs;
    else {
      deltaCompensationUs = frameUs;
    }
    deltaCompensation = microseconds(deltaCompensationUs);
    targetTimeDelta -= deltaCompensation;
  }
}

bool MatrixVideoPlayer::debugLoad(size_t numFrames) {
  clear();

  return false;
  /*
  frames.resize(numFrames);
  for (size_t i=0; i<frames.size(); ++i) {
      frames[i].id = i;
      frames[i].pixels.resize(36*24, RGB(0,0,0));
      for (size_t y = 0; y<36; y++) {
          for (size_t x = 0; x < i % 24; x++) {
              frames[i].pixels(y*24 + x) = RGB(1,1,1);
          }
      }
  }
  frameTime = microseconds(33333); // 33.333 ms
  state = STOPPED;
  notifyListenersState(state);
  return true;
  */
}

void MatrixVideoPlayer::debugSetFrameTime(double timeSec) {
  frameTime = microseconds((size_t)(timeSec * 1e+6));
}

void MatrixVideoPlayer::notifyListenersState(eState state) {
  for (auto listener : listeners) {
    listener->onStateChanged(state);
  }
}

void MatrixVideoPlayer::notifyListenersTime(double time) {
  for (auto listener : listeners) {
    listener->onTimeChanged(time);
  }
}

void MatrixVideoPlayer::notifyListenersTrackEnd() {
  for (auto listener : listeners) {
    listener->onTrackEnded();
  }
}

void MatrixVideoPlayer::notifyListenersFrame(const QImage& frame) {
  for (auto listener : listeners) {
    listener->onFrameChanged(frame);
  }
}

void MatrixVideoPlayer::addListener(MatrixVideoPlayerListener* listener) {
  listeners.insert(listener);
}

void MatrixVideoPlayer::removeListener(MatrixVideoPlayerListener* listener) {
  listeners.erase(listener);
}
