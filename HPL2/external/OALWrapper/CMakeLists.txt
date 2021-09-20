cmake_minimum_required (VERSION 3.0)
project (OALWrapper)

set(OALWrapper_VERSION_MAJOR 1)
set(OALWrapper_VERSION_MINOR 0)

SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts)

FIND_PACKAGE(OGG REQUIRED)
FIND_PACKAGE(Vorbis REQUIRED)
IF(NOT(APPLE))
  FIND_PACKAGE(VorbisFile REQUIRED)
ENDIF()
FIND_PACKAGE(OpenAL REQUIRED)
FIND_PACKAGE(SDL2 REQUIRED)

LIST(APPEND OALWRAPPER_INCLUDE_PATHS
  PRIVATE ${OGG_INCLUDE_DIR}
  PRIVATE ${VORBIS_INCLUDE_DIR}
  PRIVATE ${OPENAL_INCLUDE_DIR}
  PRIVATE ${SDL2_INCLUDE_DIR}
  PUBLIC include
  )

if(VORBISFILE_INCLUDE_DIR)
  LIST(APPEND OALWRAPPER_INCLUDE_PATHS
    PRIVATE ${VORBISFILE_INCLUDE_DIR}
    )
endif()

SET(all_sources
    sources/OAL_AudioData.cpp
    sources/OAL_Buffer.cpp
    sources/OAL_Device.cpp
    sources/OAL_Effect.cpp
    sources/OAL_Effect_Reverb.cpp
    sources/OAL_EffectSlot.cpp
    sources/OAL_EFX.cpp
    sources/OAL_EFXManager.cpp
    sources/OAL_Filter.cpp
    sources/OAL_Helper.cpp
    sources/OAL_Init.cpp
    sources/OAL_Loaders.cpp
    sources/OAL_LoggerObject.cpp
    sources/OAL_OggSample.cpp
    sources/OAL_OggStream.cpp
    sources/OAL_WAVSample.cpp
    sources/OAL_CustomStream.cpp
    sources/OAL_Playback.cpp
    sources/OAL_Sample.cpp
    sources/OAL_Source.cpp
    sources/OAL_SourceManager.cpp
    sources/OAL_Stream.cpp
    sources/OAL_Types.cpp
)

SET(all_includes
    include//OALWrapper/OAL_AudioData.h
    include//OALWrapper/OAL_Buffer.h
    include//OALWrapper/OAL_CustomStream.h
    include//OALWrapper/OAL_Device.h
    include//OALWrapper/OAL_Effect.h
    include//OALWrapper/OAL_Effect_Reverb.h
    include//OALWrapper/OAL_EffectSlot.h
    include//OALWrapper/OAL_EFX.h
    include//OALWrapper/OAL_EFXManager.h
    include//OALWrapper/OAL_Filter.h
    include//OALWrapper/OAL_Funcs.h
    include//OALWrapper/OAL_Helper.h
    include//OALWrapper/OAL_Init.h
    include//OALWrapper/OAL_Loaders.h
    include//OALWrapper/OAL_LoggerObject.h
    include//OALWrapper/OAL_LowLevelObject.h
    include//OALWrapper/OAL_OggSample.h
    include//OALWrapper/OAL_OggStream.h
    include//OALWrapper/OAL_Playback.h
    include//OALWrapper/OAL_Sample.h
    include//OALWrapper/OAL_Source.h
    include//OALWrapper/OAL_SourceManager.h
    include//OALWrapper/OAL_Stream.h
    include//OALWrapper/OAL_Types.h
    include//OALWrapper/OAL_WAVSample.h
)

add_definitions(-DUSE_SDL2)

add_library(OALWrapper STATIC
    ${all_sources}
    ${all_includes}
)

TARGET_LINK_LIBRARIES(OALWrapper
    ${VORBISFILE_LIBRARY}
    ${VORBIS_LIBRARY}
    ${OGG_LIBRARY}
    ${OPENAL_LIBRARY}
    ${SDL2_LIBRARY}
)
TARGET_INCLUDE_DIRECTORIES(OALWrapper
    ${OALWRAPPER_INCLUDE_PATHS}
)

ADD_EXECUTABLE(SimpleTest EXCLUDE_FROM_ALL tests/Simple/main.cpp)
TARGET_LINK_LIBRARIES(SimpleTest OALWrapper)

ADD_EXECUTABLE(PlaySound EXCLUDE_FROM_ALL tests/PlaySound/main.cpp)
TARGET_LINK_LIBRARIES(PlaySound OALWrapper)

ADD_EXECUTABLE(Playlist EXCLUDE_FROM_ALL tests/Playlist/main.cpp)
TARGET_LINK_LIBRARIES(Playlist OALWrapper)

ADD_EXECUTABLE(CustomStream EXCLUDE_FROM_ALL tests/CustomStream/main.cpp)
TARGET_LINK_LIBRARIES(CustomStream OALWrapper)

# Exports
SET(OALWRAPPER_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
