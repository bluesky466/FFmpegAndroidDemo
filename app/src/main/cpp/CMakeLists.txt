cmake_minimum_required(VERSION 3.18.1)

project("ffmpegdemo")

add_library(ffmpegdemo SHARED ffmpeg_demo.cpp video_sender.cpp opengl_display.cpp egl_helper.cpp video_decoder.cpp)

find_library(log-lib log)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/include)

# ffmpeg库依赖
add_library(avcodec SHARED IMPORTED)
set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libavcodec.so)

add_library(avfilter SHARED IMPORTED)
set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libavfilter.so)

add_library(avformat SHARED IMPORTED)
set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libavformat.so)

add_library(avutil SHARED IMPORTED)
set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libavutil.so)

add_library(swresample SHARED IMPORTED)
set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libswresample.so)

add_library(swscale SHARED IMPORTED)
set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../../../jniLibs/${ANDROID_ABI}/libswscale.so)

target_link_libraries(
        ffmpegdemo

        # log
        ${log-lib}

        EGL
        GLESv2
        android

        # FFmpeg libs
        avcodec
        avfilter
        avformat
        avutil
        swresample
        swscale
)