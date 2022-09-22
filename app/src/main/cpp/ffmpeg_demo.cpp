#include <jni.h>
#include <android/log.h>

#include "video_sender.h"
#include "opengl_display.h"
#include "egl_helper.h"
#include "video_decoder.h"
#include <unistd.h>

extern "C" {
#include <libavcodec/codec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
}

static const char *TAG = "FFmpegDemo";
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)

extern "C" JNIEXPORT void JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_send(
        JNIEnv *env,
        jobject /* this */,
        jstring srcFile,
        jstring destUrl) {
    const char *src = env->GetStringUTFChars(srcFile, NULL);
    const char *dest = env->GetStringUTFChars(destUrl, NULL);
    LOGD("send: %s -> %s", src, dest);
    VideoSender::Send(src, dest);
}

extern "C" JNIEXPORT void JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_play(
        JNIEnv *env,
        jobject /* this */,
        jstring url,
        jobject jSurface,
        jint width,
        jint height) {
    const char *urlStr = env->GetStringUTFChars(url, NULL);

    VideoDecoder decoder;
    decoder.Load(urlStr);
    LOGD("play %s %d*%d, %d*%d", urlStr, width, height, decoder.GetVideoWidth(), decoder.GetVideoHeight());

    if(decoder.GetPixelFormat() != AV_PIX_FMT_YUV420P) {
        LOGD("only support yuv420, the video format is %d", decoder.GetPixelFormat());
        return;
    }

    EGLHelper eglHelper;
    eglHelper.Init(env, jSurface);

    OpenGlDisplay display;
    bool result = display.Init(width, height, decoder.GetVideoWidth(), decoder.GetVideoHeight());

    AVFrame *frame;
    while ((frame = decoder.NextFrame()) != NULL) {
        eglHelper.MakeCurrent();
        display.Render(frame->data, frame->linesize);
        eglHelper.SwapBuffers();
    }

    decoder.Release();
    display.Destroy();
    eglHelper.Destroy();
    LOGD("play: finish %d", result);
}