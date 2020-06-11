include(FindPackageHandleStandardArgs)

find_path(FMOD_INCLUDE_DIR
        NAMES fmod.hpp
	PATHS
	"c:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/lowlevel/inc/"
)

find_library(FMOD_LIBRARY
	NAMES fmod64_vc
	PATHS
	"c:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api/lowlevel/lib/"
)

find_package_handle_standard_args(FMOD DEFAULT_MSG FMOD_INCLUDE_DIR FMOD_LIBRARY)

if(FMOD_FOUND)
    set(FMOD_INCLUDE_DIRS ${FMOD_INCLUDE_DIR})
    set(FMOD_LIBRARIES ${FMOD_LIBRARY})
    mark_as_advanced(FMOD_INCLUDE_DIR FMOD_LIBRARY)
endif()


