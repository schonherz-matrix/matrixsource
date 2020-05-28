#include "Q4XLoader.h"

#include <QByteArray>
#include <cstdint>
#include <fstream>
#include <iostream>

using namespace std;
using namespace std::chrono;

Q4XLoader::Q4XLoader() {
  isResampled = false;
  soundDataSize = 0;
  width_ = 0;
  height_ = 0;
}

bool ReadCompressed(istream& is, vector<uint8_t>& output);

bool Q4XLoader::load(std::string file) {
  // open given file
  ifstream inputFile(file, ios::binary | ios::in);
  if (!inputFile.is_open()) {
    cout << "Could not open file." << endl;
    return false;
  }
  std::vector<uint8_t> buffer(4);

  // read magic header
  if (!inputFile.read((char*)buffer.data(), 4)) {
    return false;
  }

  auto magic = string(buffer.data(), buffer.data() + 4);
  if (magic != "Q4X1" && magic != "Q4X2") {
    cout << "Not Q4X." << endl;
    return false;
  }

  // read dimensions of the video
  buffer.resize(4);
  if (!inputFile.read((char*)buffer.data(), 4)) {
    return false;
  }
  uint16_t width = uint16_t(buffer[0] << 8) | buffer[1];
  uint16_t height = uint16_t(buffer[2] << 8) | buffer[3];
  this->width_ = width;
  this->height_ = height;

  // uncompress chunks
  std::vector<uint8_t> qp4, qpr, sound;
  if (!ReadCompressed(inputFile, qp4) || !ReadCompressed(inputFile, qpr)) {
    cout << "Uncompressing failed." << endl;
    return false;
  }

  // TODO: SOUND IS NOT COMPRESSED
  // if (ReadCompressed(inputFile, sound)) {
  //    cout << "Sound file succesfully loaded." << endl;
  //}
  size_t currentPos = inputFile.tellg();
  inputFile.seekg(0, ios::end);
  size_t fileSize = inputFile.tellg();
  inputFile.seekg(currentPos, ios::beg);
  soundDataSize = fileSize - currentPos;
  if (soundDataSize > 4) {
    soundDataSize -= 4;  // chop off size in q4x

    // read sound file's size
    unsigned char cSoundFileSize[4];
    inputFile.read(reinterpret_cast<char*>(cSoundFileSize), 4);
    uint32_t uSoundFileSize =
        (uint32_t)cSoundFileSize[0] << 24 | (uint32_t)cSoundFileSize[1] << 16 |
        (uint32_t)cSoundFileSize[2] << 8 | cSoundFileSize[3];
    cout << "Sound file of " << uSoundFileSize << " found." << endl;
    if (uSoundFileSize <= soundDataSize) {
      soundDataSize = uSoundFileSize;
      soundData.reset(operator new(soundDataSize));
      inputFile.read((char*)soundData.get(), soundDataSize);
    } else {
      soundData.reset();
      soundDataSize = 0;
    }
  } else {
    soundData.reset();
    soundDataSize = 0;
  }

  // parse frames from qpr
  if (qpr.size() < 8) {
    cout << "Invalid qpr file." << endl;
    return false;
  }
  string magicHeader(qpr.data(), qpr.data() + 7);
  if (magicHeader != "qpr v1\n") {
    cout << "Incorrect qpr header." << endl;
    return false;
  }
  string title, audio, length;
  size_t index = 7;
  while (index < qpr.size() && qpr[index] != '\n') {
    title += qpr[index];
    ++index;
  }
  ++index;
  while (index < qpr.size() && qpr[index] != '\n') {
    audio += qpr[index];
    ++index;
  }
  ++index;
  while (index < qpr.size() && qpr[index] != '\n') {
    length += qpr[index];
    ++index;
  }
  ++index;

  QImage frame(width, height, QImage::Format_RGB888);
  while (index + height * width * 3 + 4 < qpr.size()) {
    for (int x = 0; x < width; x++) {
      for (int y = 0; y < height; y++) {
        frame.setPixelColor(x, y,
                            QColor(qpr[index + 3 * (x + y * width) + 0],
                                   qpr[index + 3 * (x + y * width) + 1],
                                   qpr[index + 3 * (x + y * width) + 2]));
      }
    }
    index += height * width * 3;
    uint32_t delay = qpr[index + 0] << 24 | qpr[index + 1] << 16 |
                     qpr[index + 2] << 8 | qpr[index + 3];
    index += 4;
    if (delay % 20 != 0) {
      cout << "Frame has invalid delay." << endl;
      return false;
    }

    for (unsigned frameCount = 0; frameCount < delay; frameCount += 20) {
      originalFrames.push_back(frame);
    }
  }
  if (originalFrames.size() == 0) {
    return false;
  }

  cout << title << ", " << audio << ", " << length << endl;
  cout << "Number of frames: " << originalFrames.size();
  originalFrameTime = microseconds(20 * 1000);

  return true;
}

bool ReadCompressed(istream& is, vector<uint8_t>& output) {
  uint32_t chunkSize;
  output.resize(4);
  if (!is.read((char*)output.data(), 4)) {
    return false;
  }
  chunkSize = uint32_t(output[0] << 24) | uint32_t(output[1] << 16) |
              uint32_t(output[2] << 8) | output[3];
  cout << "Size given in file: " << chunkSize << endl;

  output.resize(chunkSize + 4);
  if (!is.read((char*)output.data() + 4, chunkSize)) {
    return false;
  }
  uint32_t expsize = chunkSize * 9;
  output[0] = (expsize >> 24) & 0xFF;
  output[1] = (expsize >> 16) & 0xFF;
  output[2] = (expsize >> 8) & 0xFF;
  output[3] = (expsize >> 0) & 0xFF;

  QByteArray uncompressed = qUncompress(output.data(), output.size());
  cout << "Real size of uncompressed data: " << uncompressed.size() << endl;
  if (uncompressed.size() == 0) {
    return false;
  }
  output.assign(uncompressed.begin(), uncompressed.end());

  return true;
}

void Q4XLoader::clear() {
  isResampled = false;
  resampledFrames.clear();
  originalFrames.clear();
}

const std::vector<QImage>& Q4XLoader::getFrames() const {
  if (isResampled) {
    return resampledFrames;
  } else {
    return originalFrames;
  }
}

microseconds Q4XLoader::getFrameTime() const {
  if (isResampled) {
    return resampledFrameTime;
  } else {
    return originalFrameTime;
  }
}

const void* Q4XLoader::getSoundData() const { return soundData.get(); }

size_t Q4XLoader::getSoundDataSize() const { return soundDataSize; }
