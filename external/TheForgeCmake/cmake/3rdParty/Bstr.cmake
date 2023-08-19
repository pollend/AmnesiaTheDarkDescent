if(TARGET Bstr)
    return()
endif()

set(BSTR_FILES 
    "${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c")
add_library(Bstr STATIC ${BSTR_FILES})
target_include_directories(Bstr PUBLIC ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib)