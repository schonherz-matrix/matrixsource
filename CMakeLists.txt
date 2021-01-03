cmake_minimum_required(VERSION 3.16)

project(
  matrixsource
  DESCRIPTION "Schönherz Mátrix animation player for q4x"
  HOMEPAGE_URL "kszk.bme.hu"
  LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_compile_definitions(LIBMUEB_AS_DLL)

find_package(
  Qt5
  COMPONENTS Widgets
  REQUIRED)

if(NOT DEFINED MATRIX_SIMPLE)
  find_package(libmueb 2.0 REQUIRED)
endif()

find_package(FMOD REQUIRED)

add_executable(
  ${PROJECT_NAME} WIN32
  src/main.cpp
  src/MatrixAudioPlayer.cpp
  src/MatrixAudioPlayer.h
  src/MatrixPlayer.cpp
  src/MatrixPlayer.h
  src/MatrixPlayerWindow.cpp
  src/MatrixPlayerWindow.h
  src/MatrixPlayerWindow.ui
  src/MatrixVideoPlayer.cpp
  src/MatrixVideoPlayer.h
  src/Q4XLoader.cpp
  src/Q4XLoader.h)

target_include_directories(${PROJECT_NAME} PRIVATE ${FMOD_INCLUDE_DIRS})
target_link_libraries(
  ${PROJECT_NAME} PRIVATE Qt5::Widgets libmueb::muebtransmitter
                          ${FMOD_LIBRARIES})
