#-------------------------------------------------
#
# Project created by QtCreator 2015-09-18T21:52:46
#
#-------------------------------------------------

QT += gui widgets network

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS  += \
    src/MatrixAudioPlayer.h \
    src/MatrixPlayer.h \
    src/MatrixPlayerWindow.h \
    src/MatrixVideoPlayer.h \
    src/Q4XLoader.h

SOURCES += \
    src/main.cpp \
    src/MatrixPlayerWindow.cpp \
    src/MatrixVideoPlayer.cpp \
    src/MatrixPlayer.cpp \
    src/Q4XLoader.cpp \
    src/MatrixAudioPlayer.cpp

FORMS    += \
    src/MatrixPlayerWindow.ui

win32: {
    LIBS += -L'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/lowlevel/lib/' -lfmod64_vc

    INCLUDEPATH += 'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/lowlevel/inc'
    DEPENDPATH += 'C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/lowlevel/inc'
}

unix: LIBS += -lfmod

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../libmueb/release/ -lmueb
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../libmueb/debug/ -lmueb
else:unix: LIBS += -L$$PWD/../libmueb/ -lmueb

INCLUDEPATH += $$PWD/../libmueb/include/libmueb
DEPENDPATH += $$PWD/../libmueb/include/libmueb
