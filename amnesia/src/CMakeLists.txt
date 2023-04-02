cmake_minimum_required (VERSION 3.0)
project(Lux)


if(LINUX)
    # Well, first of all, disable some harmless warnings, which are really
    # hard to get rid off (they are overall in the code).
    set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -Wno-switch -Wno-undefined-var-template -Wno-extern-c-compat")
    set(CMAKE_CXX_FLAGS_RELEASE "-fno-strict-aliasing")

    set(AMNESIA_GAME_DIRECTORY "$ENV{HOME}/.local/share/Steam/steamapps/common/Amnesia The Dark Descent" CACHE PATH "game path")
else()
    set(AMNESIA_GAME_DIRECTORY "" CACHE PATH "")
endif()


option(USERDIR_RESOURCES "Allow using local userdirectory resources" ON)
option(SYSTEMWIDE_RESOURCES "Allow system-wide installation of game resources" OFF)
set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2010-2013 Frictional Games")

if (NOT DEFINED SYSTEMWIDE_RESOURCES_LOCATION)
  set (SYSTEMWIDE_RESOURCES_LOCATION "/usr/local/share/amnesia-tdd")
endif (NOT DEFINED SYSTEMWIDE_RESOURCES_LOCATION)


if(USERDIR_RESOURCES)
    add_definitions(
        -DUSERDIR_RESOURCES
    )
endif()

set(VERSION "1.3.1")

add_subdirectory(game game)

add_custom_target(GameRelease
    DEPENDS Amnesia
)

set(AMNESIA_EXECUTABLE_OUTPUT_PATH  ${INSTALL_OUTPUT_PATH}/Amnesia)
install(DIRECTORY DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})

if(WITH_TOOLS)
    install (TARGETS MapViewer RUNTIME
             DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS ModelViewer RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS MshConverter RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS LevelEditor RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS ModelEditor RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS ParticleEditor RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
    install (TARGETS MaterialEditor RUNTIME
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
endif()
if(WIN32)
    install (TARGETS Amnesia 
        RUNTIME_DEPENDENCIES
        PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
        POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
        RUNTIME DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH}
        LIBRARY DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
else()
    install (TARGETS Amnesia
        RUNTIME DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH}
        LIBRARY DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})
endif()

install(FILES
    $<TARGET_RUNTIME_DLLS:Amnesia>
    DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})

if((NOT ${AMNESIA_GAME_DIRECTORY} STREQUAL "") AND COPY_GAME_ASSETS)
    install(DIRECTORY "${AMNESIA_GAME_DIRECTORY}/"
            DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH}
            FILES_MATCHING
            PATTERN "Amnesia.*" EXCLUDE
            PATTERN "Amnesia_*" EXCLUDE
            PATTERN "Launcher.*" EXCLUDE
            PATTERN "*.rar" EXCLUDE
            PATTERN "*.pdf" EXCLUDE
            PATTERN "*.dll" EXCLUDE
            PATTERN "*.exe" EXCLUDE
            PATTERN "*"
            )
endif()
