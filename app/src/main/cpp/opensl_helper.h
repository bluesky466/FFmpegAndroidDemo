#ifndef __OPENSL_HELPER_H__
#define __OPENSL_HELPER_H__

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <android/log.h>
#include <pthread.h>

#define LOG(...) __android_log_print(ANDROID_LOG_DEBUG, "OpenSLDemo-JNI", __VA_ARGS__)

typedef struct {
    pthread_mutex_t m;
    pthread_cond_t c;
    unsigned char s;
} ThreadLock;

typedef struct {
    SLObjectItf engineObject;
    SLEngineItf engineInterface;

    SLObjectItf recorderObject;
    SLRecordItf recorderInterface;

    SLObjectItf outputMixObject;

    SLObjectItf playerObject;
    SLPlayItf playInterface;

    SLAndroidSimpleBufferQueueItf queueInterface;

    SLDataLocator_AndroidSimpleBufferQueue queue;
    SLDataFormat_PCM format;
    SLDataLocator_OutputMix outputMix;
    SLDataLocator_IODevice device;

    SLDataSource source;
    SLDataSink sink;

    ThreadLock threadLock;
} OpenSLHelper;

void lockInit(ThreadLock* pThreadLock);
void lockWait(ThreadLock* pThreadLock);
void lockNotify(ThreadLock* pThreadLock);
void lockDestroy(ThreadLock* pThreadLock);

void openSLHelperInit(OpenSLHelper* pHelper);
void openSLHelperDestroy(OpenSLHelper* pHelper);

void recorderInit(OpenSLHelper* pHelper, SLuint32 numChannels, int samplingRate);

void playerInit(OpenSLHelper* pHelper, SLuint32 numChannels, int samplingRate);

#endif /* ifndef OPENSL_HELPER_H_ */