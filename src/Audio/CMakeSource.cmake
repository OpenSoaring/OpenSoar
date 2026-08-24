set(_SOURCES
        Audio/Settings.cpp
        Audio/Sound.cpp
        Audio/VarioSettings.cpp
)

# mirror upstream build/libaudio.mk: the PCM stack is only built when a
# player backend exists (SDL, ALSA or Android) - see Audio/Features.hpp
if(ENABLE_SDL OR ENABLE_ALSA OR ANDROID)
  set(HAVE_PCM_PLAYER ON)
endif()

if(HAVE_PCM_PLAYER)
  list(APPEND _SOURCES
        Audio/ToneSynthesiser.cpp
        Audio/VarioAudioValue.cpp
        Audio/VarioSynthesiser.cpp
        Audio/PCMPlayer.cpp
        Audio/VarioGlue.cpp
  )
  if(NOT ANDROID)  # PCM mixer: SDL/ALSA only (HAVE_PCM_MIXER)
    list(APPEND _SOURCES
        Audio/GlobalPCMResourcePlayer.cpp
        Audio/GlobalPCMMixer.cpp
        Audio/GlobalVolumeController.cpp
        Audio/MixerPCMPlayer.cpp
        Audio/PCMBufferDataSource.cpp
        Audio/PCMMixerDataSource.cpp
        Audio/PCMMixer.cpp
        Audio/PCMResourcePlayer.cpp
        Audio/VolumeController.cpp
    )
    if(ENABLE_ALSA)
      list(APPEND _SOURCES Audio/ALSAEnv.cpp Audio/ALSAPCMPlayer.cpp)
    elseif(ENABLE_SDL)
      list(APPEND _SOURCES Audio/SDLPCMPlayer.cpp)
    endif()
  endif()
endif()

set(SCRIPT_FILES
    CMakeSource.cmake
)
