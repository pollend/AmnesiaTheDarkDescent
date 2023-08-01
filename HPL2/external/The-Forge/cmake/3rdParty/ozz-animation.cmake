if(TARGET ozz-animation)
    return()
endif()

file(GLOB OZZ_ANIMATION_SOURCES 
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/animation/offline/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/animation/runtime/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/base/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/base/containers/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/base/io/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/base/maths/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/base/memory/*.cc
    ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/src/geometry/runtime/*.cc)

add_library(ozz-animation STATIC ${OZZ_ANIMATION_SOURCES})
target_include_directories(ozz-animation PUBLIC ${THE_FORGE_DIR}/Common_3/Resources/AnimationSystem/ThirdParty/OpenSource/ozz-animation/include)


