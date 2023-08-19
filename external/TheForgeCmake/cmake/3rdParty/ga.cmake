if(TARGET gainput)
    return()
endif()

file(GLOB GAINPUT_SOURCES 
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/builtin/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/dev/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/gestures/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/keyboard/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/mouse/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/pad/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/recorder/*.cpp
    )


if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    file(GLOB GAINPUT_SOURCES_MACOS
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParts/OpenSource/gainput/lib/source/hidapi/mac/*.c
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParts/OpenSource/gainput/lib/source/gainput/*.mm)
    list(APPEND GAINPUT_SOURCES ${GAINPUT_SOURCES_MACOS})
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
    file(GLOB GAINPUT_SOURCES_WINDOWS
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParts/OpenSource/gainput/lib/source/hidapi/windows/*.c
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/hid/*.cpp
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParty/OpenSource/gainput/lib/source/gainput/hid/hidparsers/*.cpp
    )
    list(APPEND GAINPUT_SOURCES ${GAINPUT_SOURCES_WINDOWS})
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    file(GLOB GAINPUT_SOURCES_LINUX
        ${THE_FORGE_DIR}/Common_3/Application/ThirdParts/OpenSource/gainput/lib/source/hidapi/linux/*.c)
    list(APPEND GAINPUT_SOURCES ${GAINPUT_SOURCES_LINUX})
endif()

add_library(ga STATIC ${GAINPUT_SOURCES})
target_include_directories(ga PUBLIC ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/lua)


