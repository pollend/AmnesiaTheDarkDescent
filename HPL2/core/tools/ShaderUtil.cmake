
function( hpl_add_shader ARG_OUT FILE FOLDER)
    cmake_parse_arguments( ARG "TARGET_NAME" "DEFINES;INCLUDES" "" ${ARGN} )
    get_filename_component( FILENAME "${FILE}" NAME_WE )
    string( SUBSTRING "${FILENAME}" 0 2 TYPE )
    if( "${TYPE}" STREQUAL "fs" )
        set( TYPE "FRAGMENT" )
        set( D3D_PREFIX "ps" )
    elseif( "${TYPE}" STREQUAL "vs" )
        set( TYPE "VERTEX" )
        set( D3D_PREFIX "vs" )
    elseif( "${TYPE}" STREQUAL "cs" )
        set( TYPE "COMPUTE" )
        set( D3D_PREFIX "cs" )
    else()
        set( TYPE "" )
    endif()
    
    if(ARG_TARGET_NAME)
        set(FILENAME ${TARGET_NAME})
    endif()    

    if( NOT "${TYPE}" STREQUAL "" )
        set(COMMON FILE ${FILE} ${TYPE}) 
        set( OUTPUTS "" )
        set( OUTPUTS_PRETTY "" )
        
        if(ARG_DEFINES)
            list(JOIN ${ARG_DEFINES} ";" SHADER_DEFINES)
            list(APPEND COMMON DEFINES "${SHADER_DEFINES}")
        endif()

        if(ARG_INCLUDES)
            list(JOIN ${ARG_INCLUDES} ";" SHADER_INCLUDE)
            list(APPEND COMMON INCLUDES "${SHADER_INCLUDE}" )
        endif()
        list(APPEND COMMON INCLUDES "${BGFX_DIR}/src" )

        if( WIN32 )
            # dx9
            if( NOT "${TYPE}" STREQUAL "COMPUTE" )
                set( DX9_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/dx9/${FILENAME}.bin )
                shaderc_parse( DX9 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_3_0 O 3 OUTPUT ${DX9_OUTPUT} )
                list( APPEND OUTPUTS "DX9" )
                set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}DX9, " )
            endif()

            # dx11
            set( DX11_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/dx11/${FILENAME}.bin )
            if( NOT "${TYPE}" STREQUAL "COMPUTE" )
                shaderc_parse( DX11 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_5_0 O 3 OUTPUT ${DX11_OUTPUT} )
            else()
                shaderc_parse( DX11 ${COMMON} WINDOWS PROFILE ${D3D_PREFIX}_5_0 O 1 OUTPUT ${DX11_OUTPUT} )
            endif()
            list( APPEND OUTPUTS "DX11" )
            set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}DX11, " )
        endif()

        if( APPLE )
            # metal
            set( METAL_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/metal/${FILENAME}.bin )
            shaderc_parse( METAL ${COMMON} OSX PROFILE metal OUTPUT ${METAL_OUTPUT} )
            list( APPEND OUTPUTS "METAL" )
            set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}Metal, " )
        endif()

        # essl
        if( NOT "${TYPE}" STREQUAL "COMPUTE" )
            set( ESSL_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/essl/${FILENAME}.bin )
            shaderc_parse( ESSL ${COMMON} ANDROID OUTPUT ${ESSL_OUTPUT} )
            list( APPEND OUTPUTS "ESSL" )
            set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}ESSL, " )
        endif()

        # glsl
        set( GLSL_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/glsl/${FILENAME}.bin )
        if( NOT "${TYPE}" STREQUAL "COMPUTE" )
            shaderc_parse( GLSL ${COMMON} LINUX PROFILE 120 OUTPUT ${GLSL_OUTPUT} )
        else()
            shaderc_parse( GLSL ${COMMON} LINUX PROFILE 430 OUTPUT ${GLSL_OUTPUT} )
        endif()
        list( APPEND OUTPUTS "GLSL" )
        set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}GLSL, " )

        # spirv
        if( NOT "${TYPE}" STREQUAL "COMPUTE" )
            set( SPIRV_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/spirv/${FILENAME}.bin )
            shaderc_parse( SPIRV ${COMMON} LINUX PROFILE spirv OUTPUT ${SPIRV_OUTPUT} )
            list( APPEND OUTPUTS "SPIRV" )
            set( OUTPUTS_PRETTY "${OUTPUTS_PRETTY}SPIRV" )
            set( OUTPUT_FILES "" )
            set( COMMANDS "" )
        endif()

        foreach( OUT ${OUTPUTS} )
            list( APPEND OUTPUT_FILES ${${OUT}_OUTPUT})
            list( APPEND COMMANDS COMMAND "$<TARGET_FILE:shaderc>" ${${OUT}} )
            get_filename_component( OUT_DIR ${${OUT}_OUTPUT} DIRECTORY )
            file( MAKE_DIRECTORY ${OUT_DIR} )
        endforeach()

        file( RELATIVE_PATH PRINT_NAME ${CMAKE_CURRENT_BINARY_DIR} ${FILE} )
        add_custom_command(
            MAIN_DEPENDENCY
            ${FILE}
            OUTPUT
            ${OUTPUT_FILES}
            ${COMMANDS}
            COMMENT "Compiling shader ${PRINT_NAME} for ${OUTPUTS_PRETTY}"
        )

        # if(ARG_OUT)
            # list(APPEND ARG_OUT ${OUTPUT_FILES})
        
        # message(STATUS "shaders: ${${ARG_OUT}}")
        set(${ARG_OUT} ${${ARG_OUT}} ${OUTPUT_FILES} PARENT_SCOPE )
        # endif()
    endif()
endfunction()