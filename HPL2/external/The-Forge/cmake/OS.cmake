set(FORGE_OS_DIR ../The-Forge/Common_3/OS)

set(FORGE_APP_DIR ../The-Forge/Common_3/Application)

set(FORGE_UTIL_DIR ../The-Forge/Common_3/Utilities)

set(FORGE_APP_FILES
        ${FORGE_APP_DIR}/CameraController.cpp
        ${FORGE_APP_DIR}/Config.h
        ${FORGE_APP_DIR}/DLL.h
        ${FORGE_APP_DIR}/InputSystem.cpp
        ${FORGE_APP_DIR}/Screenshot.cpp
        ${FORGE_APP_DIR}/Fonts/FontSystem.cpp
        ${FORGE_APP_DIR}/Fonts/stbtt.cpp
        ${FORGE_APP_DIR}/Profiler/GpuProfiler.cpp
        ${FORGE_APP_DIR}/Profiler/GpuProfiler.h
        ${FORGE_APP_DIR}/Profiler/ProfilerBase.cpp
        ${FORGE_APP_DIR}/Profiler/ProfilerBase.h
        ${FORGE_APP_DIR}/Profiler/ProfilerHTML.h
        ${FORGE_APP_DIR}/UI/UI.cpp
        )

set(FORGE_OS_CORE_FILES
        ${FORGE_OS_DIR}/Interfaces/IOperatingSystem.h
        ${FORGE_OS_DIR}/CPUConfig.cpp
        ${FORGE_OS_DIR}/CPUConfig.h
        ${FORGE_OS_DIR}/WindowSystem/WindowSystem.cpp
        ${FORGE_UTIL_DIR}/Timer.c
        )

set(FORGE_OS_WINDOWS_FILES
        ${FORGE_OS_DIR}/Windows/WindowsBase.cpp
        ${FORGE_OS_DIR}/Windows/WindowsFileSystem.cpp
        ${FORGE_OS_DIR}/Windows/WindowsLog.c
        ${FORGE_OS_DIR}/Windows/WindowsStackTraceDump.cpp
        ${FORGE_OS_DIR}/Windows/WindowsStackTraceDump.h
        ${FORGE_OS_DIR}/Windows/WindowsThread.c
        ${FORGE_OS_DIR}/Windows/WindowsTime.c
        ${FORGE_OS_DIR}/Windows/WindowsWindow.cpp)

set(FORGE_OS_DARWIN_FILES
        ${FORGE_OS_DIR}/Darwin/CocoaFileSystem.mm
        ${FORGE_OS_DIR}/Darwin/DarwinLog.c
        ${FORGE_OS_DIR}/Darwin/DarwinThread.c
        ../The-Forge/Common_3/Utilities/FileSystem/UnixFileSystem.cpp)

set(FORGE_OS_APPLE_FILES
        ${FORGE_OS_DIR}/Darwin/macOSAppDelegate.h
        ${FORGE_OS_DIR}/Darwin/macOSAppDelegate.m
        ${FORGE_OS_DIR}/Darwin/macOSBase.mm
        ${FORGE_OS_DIR}/Darwin/macOSWindow.mm
        )

if (${WINDOWS} MATCHES ON)
    set(FORGE_OS_SPECIFIC_FILES ${FORGE_OS_SPECIFIC_FILES} ${FORGE_OS_WINDOWS_FILES})
endif ()

if (${APPLE_PLATFORM} MATCHES ON)
    set(FORGE_OS_SPECIFIC_FILES ${FORGE_OS_SPECIFIC_FILES} ${FORGE_OS_DARWIN_FILES} ${FORGE_OS_APPLE_FILES})
endif ()

set(FORGE_FILESYSTEM_FILES
        ${FORGE_UTIL_DIR}/FileSystem/FileSystem.cpp
        ${FORGE_UTIL_DIR}/FileSystem/SystemRun.cpp
        ${FORGE_UTIL_DIR}/FileSystem/ZipFileSystem.c
        )

set(FORGE_MEMORY_TRACKING_FILES
        ${FORGE_UTIL_DIR}/MemoryTracking/MemoryTracking.c
        ${FORGE_UTIL_DIR}/MemoryTracking/NoMemoryDefines.h)

set(FORGE_THREADING_FILES
        ${FORGE_UTIL_DIR}/Threading/Atomics.h
        ${FORGE_UTIL_DIR}/Threading/ThreadSystem.cpp
        ${FORGE_UTIL_DIR}/Threading/ThreadSystem.h
        ${FORGE_UTIL_DIR}/Threading/UnixThreadID.h
        )

set(FORGE_MATH_FILES
        ${FORGE_UTIL_DIR}/Math/Algorithms.c
        ${FORGE_UTIL_DIR}/Math/Algorithms.h
        ${FORGE_UTIL_DIR}/Math/AlgorithmsImpl.h
        ${FORGE_UTIL_DIR}/Math/BStringHashMap.h
        ${FORGE_UTIL_DIR}/Math/MathTypes.h
        ${FORGE_UTIL_DIR}/Math/RTree.h
        ${FORGE_UTIL_DIR}/Math/StbDs.c)

set(FORGE_LOG_FILES
        ${FORGE_UTIL_DIR}/Log/Log.c
        ${FORGE_UTIL_DIR}/Log/Log.h)

set(OS_MIDDLEWARE_PARALLEL_PRIMS_FILES
        ../The-Forge/Middleware_3/ParallelPrimitives/ParallelPrimitives.cpp
        ../The-Forge/Middleware_3/ParallelPrimitives/ParallelPrimitives.h
        )

set(FORGE_GAME_FILES
        ../The-Forge/Common_3/Game/Scripting/LuaManager.cpp
        ../The-Forge/Common_3/Game/Scripting/LuaManager.h
        ../The-Forge/Common_3/Game/Scripting/LuaManagerCommon.h
        ../The-Forge/Common_3/Game/Scripting/LuaManagerImpl.cpp
        ../The-Forge/Common_3/Game/Scripting/LuaManagerImpl.h
        ../The-Forge/Common_3/Game/Scripting/LuaSystem.cpp
        ../The-Forge/Common_3/Game/Scripting/LunaV.hpp)

set(FORGE_OS_INTERFACE_FILES
        ${FORGE_UTIL_DIR}/RingBuffer.h
        ${FORGE_APP_DIR}/Interfaces/IApp.h
        ${FORGE_APP_DIR}/Interfaces/ICameraController.h
        ${FORGE_APP_DIR}/Interfaces/IFont.h
        ${FORGE_APP_DIR}/Interfaces/IInput.h
        ${FORGE_APP_DIR}/Interfaces/IMiddleware.h
        ${FORGE_APP_DIR}/Interfaces/IProfiler.h
        ${FORGE_APP_DIR}/Interfaces/IScreenshot.h
        ${FORGE_APP_DIR}/Interfaces/IUI.h
        ../The-Forge/Common_3/Game/Interfaces/IScripting.h
        ${FORGE_UTIL_DIR}/Interfaces/IFileSystem.h
        ${FORGE_UTIL_DIR}/Interfaces/ILog.h
        ${FORGE_UTIL_DIR}/Interfaces/IMemory.h
        ${FORGE_UTIL_DIR}/Interfaces/IThread.h
        ${FORGE_UTIL_DIR}/Interfaces/ITime.h
        )

