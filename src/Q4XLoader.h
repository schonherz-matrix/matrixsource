#pragma once

#include <QImage>
#include <chrono>
#include <memory>
#include <vector>

class Q4XLoader {
 public:
  Q4XLoader();

  bool load(std::string file);

  template <class Rep, class Period>
  void resample(std::chrono::duration<Rep, Period> frameTime);

  size_t width() const { return width_; }
  size_t height() const { return height_; }

  void clear();

  const std::vector<QImage>& getFrames() const;
  std::chrono::microseconds getFrameTime() const;

  const void* getSoundData() const;
  size_t getSoundDataSize() const;

 private:
  std::chrono::microseconds originalFrameTime;
  std::vector<QImage> originalFrames;
  bool isResampled;
  std::chrono::microseconds resampledFrameTime;
  std::vector<QImage> resampledFrames;

  size_t width_, height_;

  struct Deleter {
    void operator()(void* ptr) { operator delete(ptr); }
  };
  std::unique_ptr<void, Deleter> soundData;
  size_t soundDataSize;
};

template <class Rep, class Period>
void Q4XLoader::resample(std::chrono::duration<Rep, Period> frameTime) {
  if (originalFrames.size() == 0) {
    return;
  }

  resampledFrames.clear();
  resampledFrameTime = frameTime;
  isResampled = true;

  /*
  std::chrono::microseconds length = originalFrames.size() * originalFrameTime;
  std::chrono::microseconds sampleTime(0);
  while (sampleTime < length) {
      // get preceding frame in original stream
      size_t numOriginalFrames = (sampleTime.count() -
  sampleTime.count()%originalFrameTime.count()) / originalFrameTime.count() + 1;
      resampledFrames.push_back(originalFrames[numOriginalFrames - 1]);

      // increase sample time
      sampleTime += resampledFrameTime;
  }
  */
  size_t resampledFrameCount =
      (originalFrames.size() * originalFrameTime.count()) /
      resampledFrameTime.count();
  for (int i = 0; i < resampledFrameCount; i++) {
    resampledFrames.push_back(originalFrames[(i * resampledFrameTime.count()) /
                                             originalFrameTime.count()]);
  }

  // Add one blank black frame
  {
    QImage lastFrame = resampledFrames.back();
    QImage blankFrame(lastFrame.width(), lastFrame.height(),
                      QImage::Format_RGB888);
    blankFrame.fill(Qt::black);
    resampledFrames.push_back(blankFrame);
  }
}
