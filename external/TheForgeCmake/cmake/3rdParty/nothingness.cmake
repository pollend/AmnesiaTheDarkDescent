if(TARGET nothingness)
    return()
endif()

set(BSTR_SOURCE_DIR 
    ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c)
add_library(bstrlib STATIC ${BSTR_SOURCE_DIR})
