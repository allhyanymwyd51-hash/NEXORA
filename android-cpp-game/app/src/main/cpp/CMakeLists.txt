cmake_minimum_required(VERSION 3.22.1)
project("nexora_game")

set(CMAKE_CXX_STANDARD 17)

add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)

target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(nexora_game SHARED
    main.cpp)

target_include_directories(nexora_game PRIVATE
    ${ANDROID_NDK}/sources/android/native_app_glue)

target_link_libraries(nexora_game
    native_app_glue
    android
    EGL
    GLESv3
    log
    m)
