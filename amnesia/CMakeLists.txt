cmake_minimum_required (VERSION 3.0)
project(Lux)
set(VERSION "1.3.1")

option(USERDIR_RESOURCES "Allow using local userdirectory resources" ON)
option(SYSTEMWIDE_RESOURCES "Allow system-wide installation of game resources" OFF)
SET(AMNESIA_TDD_VERSION "V0000" CACHE STRING "Amnesia Release Version")

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  set(LINUX ON)
endif()


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


set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2010-2013 Frictional Games")

if (NOT DEFINED SYSTEMWIDE_RESOURCES_LOCATION)
  set (SYSTEMWIDE_RESOURCES_LOCATION "/usr/local/share/amnesia-tdd")
endif (NOT DEFINED SYSTEMWIDE_RESOURCES_LOCATION)


if(USERDIR_RESOURCES)
    add_definitions(
        -DUSERDIR_RESOURCES
    )
endif()

set(compile_defs "")
set(demo_compile_defs "LUX_DEMO_VERSION")

option(USE_GAMEPAD "Enable gamepad support" ON)
if(USE_GAMEPAD)
	list(APPEND compile_defs USE_GAMEPAD)
	list(APPEND demo_compile_defs USE_GAMEPAD)
endif()



file(GLOB AMENISA_GAME_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    game/*.cpp
    game/*.h
)
add_executable(Amnesia MACOSX_BUNDLE ${AMENISA_GAME_SOURCES})
target_link_libraries(Amnesia HPL2)
set_target_properties(Amnesia PROPERTIES
  COMPILE_DEFINITIONS "${compile_defs}"
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Game-Info.plist"
  MACOSX_BUNDLE_ICON_FILE "Lux.icns"
  MACOSX_BUNDLE_BUNDLE_VERSION ${VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${VERSION}
  MACOSX_BUNDLE_GUI_IDENTIFIER "com.frictionalgames.Amnesia"
  MACOSX_BUNDLE_DISPLAY_NAME "Amnesia"
  MACOSX_BUNDLE_BUNDLE_NAME "Amnesia: The Dark Descent"
)

IF(APPLE)
    add_definitions(
        -DMAC_OS
    )
ELSEIF(LINUX)
    add_definitions(
        -DLINUX
    )
ENDIF()
hpl_set_output_dir(Amnesia "") 

add_custom_target(GameRelease DEPENDS Amnesia)
set(AMNESIA_EXECUTABLE_OUTPUT_PATH  ${INSTALL_OUTPUT_PATH}/Amnesia)
install(DIRECTORY DESTINATION ${AMNESIA_EXECUTABLE_OUTPUT_PATH})

if(WIN32)
    add_custom_command ( TARGET Amnesia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:WinPixEventRuntime> $<TARGET_FILE_DIR:Amnesia>
    )
    add_custom_command ( TARGET Amnesia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:ags> $<TARGET_FILE_DIR:Amnesia>
    )
    add_custom_command ( TARGET Amnesia POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:DirectXShaderCompiler> $<TARGET_FILE_DIR:Amnesia>
    )
endif()

##add_custom_command(
##    TARGET Amnesia POST_BUILD
##    COMMAND ${CMAKE_COMMAND} 
##    ARGS -E copy_directory  "${_HPL2_SHADER_}" "$<TARGET_FILE_DIR:Amnesia>/Shaders"
##)
##
##add_custom_command(
##    TARGET Amnesia POST_BUILD
##    COMMAND ${CMAKE_COMMAND} 
##    ARGS -E copy_directory  "${_HPL2_COMPILEDSHADER_}" "$<TARGET_FILE_DIR:Amnesia>/CompiledShaders"
##)
copy_shader_to_target_dir(Amnesia)
add_custom_command ( TARGET Amnesia POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/gpu.data $<TARGET_FILE_DIR:Amnesia>
)

target_compile_definitions(Amnesia PUBLIC AMNESIA_TDD_VERSION="${AMNESIA_TDD_VERSION}")
target_compile_definitions(Amnesia PUBLIC AMNESIA_TDD_TAG="${AMNESIA_TDD_TAG}")

if((NOT ${AMNESIA_GAME_DIRECTORY} STREQUAL "") AND COPY_GAME_ASSETS)
    add_custom_command(TARGET Amnesia POST_BUILD COMMAND ${CMAKE_COMMAND} 
        -DAMNESIA_GAME_DIRECTORY=${AMNESIA_GAME_DIRECTORY}
        -DAMNESIA_EXECUTABLE_OUTPUT_PATH="$<TARGET_FILE_DIR:Amnesia>"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_game_directory.cmake
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
