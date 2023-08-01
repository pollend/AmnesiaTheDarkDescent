if(TARGET lua)
    return()
endif()

file(GLOB LUA_SOURCES 
#    "${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/*.c"
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lapi.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lauxlib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lbaselib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lbitlib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lcode.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lcorolib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lctype.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ldblib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ldebug.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ldo.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ldump.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lfunc.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lgc.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/linit.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/liolib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/llex.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lmathlib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lmem.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/loadlib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lobject.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lopcodes.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/loslib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lparser.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lstate.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lstring.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lstrlib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ltable.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ltablib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/ltm.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lundump.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lutf8lib.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lvm.c
    ${THE_FORGE_DIR}/Common_3/Game/ThirdParty/OpenSource/lua-5.3.5/src/lzio.c
)
add_library(lua STATIC ${LUA_SOURCES})
target_include_directories(lua PUBLIC ${THE_FORGE_DIR}/Common_3/ThirdParty/OpenSource/lua)



