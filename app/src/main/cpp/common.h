#ifndef __COMMON_H__
#define __COMMON_H__

#include <android/log.h>

static const char *TAG = "FFmpegDemo";
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)

#endif