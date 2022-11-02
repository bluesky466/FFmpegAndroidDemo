#include <jni.h>
#include <unistd.h>
#include <android/looper.h>
#include <android/log.h>

extern "C" {
#include <libavcodec/codec.h>
#include <libavformat/avio.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
}

#include "egl_helper.h"
#include "opengl_display.h"
#include "media_reader.h"
#include "opensl_helper.h"

static const char *TAG = "FFmpegDemo";
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)

using namespace std;

static int findStreamIndexByType(const MediaReader* reader, AVMediaType type) {
    vector<AVStream *> streams = reader->GetStreams();
    for (int i = 0; i < streams.size(); i++) {
        if (streams[i]->codecpar->codec_type == type) {
            return i;
        }
    }
    return -1;
}

extern "C" JNIEXPORT jlong JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_load(
        JNIEnv *env,
        jobject /* this */,
        jstring url) {
    const char *urlStr = env->GetStringUTFChars(url, NULL);

    MediaReader *reader = new MediaReader();
    reader->Load(urlStr);

    LOGD("load %s", urlStr);
    return (jlong) reader;
}

extern "C"
JNIEXPORT void JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_release(JNIEnv *env, jobject thiz, jlong readerPtr) {
    MediaReader *reader = (MediaReader *) readerPtr;
    reader->Release();
    delete reader;
}

extern "C" JNIEXPORT void JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_playVideo(
        JNIEnv *env,
        jobject /* this */,
        jlong readerPtr,
        jobject jSurface,
        jint width,
        jint height) {
    MediaReader *reader = (MediaReader *) readerPtr;
    VideoStreamDecoder videoStreamDecoder;

    int videoIndex = findStreamIndexByType(reader, AVMEDIA_TYPE_VIDEO);
    if (videoIndex >= 0) {
        videoStreamDecoder.Init(reader, videoIndex, AVPixelFormat::AV_PIX_FMT_YUV420P);
    }

    LOGD("play video %d*%d, %d*%d",
         width, height,
         videoStreamDecoder.GetVideoWidth(), videoStreamDecoder.GetVideoHeight());

    AVPixelFormat format = videoStreamDecoder.GetPixelFormat();
    if (AV_PIX_FMT_YUV420P != format) {
        LOGD("only support yuv420, the video format is %d", format);
        return;
    }

    EGLHelper eglHelper;
    eglHelper.Init(env, jSurface);

    OpenGlDisplay display;
    bool result = display.Init(
            width, height,
            videoStreamDecoder.GetVideoWidth(), videoStreamDecoder.GetVideoHeight());
    AVFrame *frame;
    while ((frame = videoStreamDecoder.NextFrame()) != NULL) {
        eglHelper.MakeCurrent();
        display.Render(frame->data, frame->linesize);
        eglHelper.SwapBuffers();
    }

    videoStreamDecoder.Destroy();
    display.Destroy();
    eglHelper.Destroy();
    LOGD("play video finish : %d", result);
}

extern "C" JNIEXPORT void JNICALL
Java_me_linjw_demo_ffmpeg_MainActivity_playAudio(
        JNIEnv *env,
        jobject /* this */,
        jlong readerPtr) {
    MediaReader *reader = (MediaReader *) readerPtr;
    AudioStreamDecoder audioStreamDecoder;

    int audioIndex = findStreamIndexByType(reader, AVMEDIA_TYPE_AUDIO);
    if (audioIndex >= 0) {
        audioStreamDecoder.Init(reader, audioIndex, AVSampleFormat::AV_SAMPLE_FMT_S16);
    }

    int sampleRate = audioStreamDecoder.GetSampleRate();
    int channelCount = audioStreamDecoder.GetChannelCount();
    int bytePerSample = audioStreamDecoder.GetBytePerSample();

    LOGD("play audio, channel count = %d, sample rate = %d", channelCount, sampleRate);

    OpenSLHelper helper;
    openSLHelperInit(&helper);
    playerInit(&helper, channelCount, sampleRate);

    u_char *buffer = NULL;
    int buffSize = -1;
    AVFrame *frame;
    while ((frame = audioStreamDecoder.NextFrame()) != NULL) {
        lockWait(&(helper.threadLock));

        int size = frame->nb_samples * channelCount * bytePerSample;
        if (buffSize < size) {
            if (NULL != buffer) {
                free(buffer);
            }
            buffer = (u_char *) calloc(size, sizeof(u_char));
            buffSize = size;
        }

        // 由于OpenSL在另外的线程播放音频
        // 为了避免在还没有播放完成的时候就调用NextFrame覆盖掉AVFrame里面的数据
        // 我们把这个数据保存到buffer中
        memcpy(buffer, frame->data[0], size);

        (*helper.queueInterface)->Enqueue(helper.queueInterface, buffer, size);
    }
    openSLHelperDestroy(&helper);
    audioStreamDecoder.Destroy();
    LOGD("play audio finish");
}