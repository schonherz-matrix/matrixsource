find_path(
  FMOD_INCLUDE_DIR
  NAMES fmod.hpp
  HINTS ENV FMOD_DIR
  PATH_SUFFIXES include/fmod include
  PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw # Fink
    /opt/local # DarwinPorts
    /opt/csw # Blastwave
    /opt
    "c:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/core/inc/"
)

find_library(
  FMOD_LIBRARY
  NAMES libfmod fmod_vc
  HINTS ENV FMOD_DIR
  PATH_SUFFIXES lib
  PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw # Fink
    /opt/local # DarwinPorts
    /opt/csw # Blastwave
    /opt
    "c:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/core/lib/x64/"
)

set(FMOD_INCLUDE_DIRS "${FMOD_INCLUDE_DIR}")
set(FMOD_LIBRARIES "${FMOD_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FMOD DEFAULT_MSG FMOD_LIBRARIES
                                  FMOD_INCLUDE_DIRS)

mark_as_advanced(FMOD_INCLUDE_DIR FMOD_LIBRARY FMOD_INCLUDE_DIRS FMOD_LIBRARIES)
