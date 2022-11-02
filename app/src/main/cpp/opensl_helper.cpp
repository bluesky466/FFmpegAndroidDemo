#include "opensl_helper.h"

static SLuint32 convertSampleRate(int sampleRate){
    switch(sampleRate) {
        case 8000:
            return SL_SAMPLINGRATE_8;
        case 11025:
            return SL_SAMPLINGRATE_11_025;
        case 16000:
            return SL_SAMPLINGRATE_16;
        case 22050:
            return SL_SAMPLINGRATE_22_05;
        case 24000:
            return SL_SAMPLINGRATE_24;
        case 32000:
            return SL_SAMPLINGRATE_32;
        case 44100:
            return SL_SAMPLINGRATE_44_1;
        case 48000:
            return SL_SAMPLINGRATE_48;
        case 64000:
            return SL_SAMPLINGRATE_64;
        case 88200:
            return SL_SAMPLINGRATE_88_2;
        case 96000:
            return SL_SAMPLINGRATE_96;
        case 192000:
            return SL_SAMPLINGRATE_192;
        default:
            return 0;
    }
}

static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf queue, void *pContext) {
    LOG("bufferQueueCallback");
    OpenSLHelper *pHelper = (OpenSLHelper *) pContext;
    lockNotify(&(pHelper->threadLock));
}

static SLuint32 getChannelMask(SLuint32 numChannels) {
    return numChannels > 1
           ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT
           : SL_SPEAKER_FRONT_CENTER;
}

void lockInit(ThreadLock *pThreadLock) {
    pthread_mutex_init(&(pThreadLock->m), NULL);
    pthread_cond_init(&(pThreadLock->c), NULL);
    pThreadLock->s = 1;
}

void lockWait(ThreadLock *pThreadLock) {
    pthread_mutex_lock(&(pThreadLock->m));
    while (!pThreadLock->s) {
        pthread_cond_wait(&(pThreadLock->c), &(pThreadLock->m));
    }
    pThreadLock->s = 0;
    pthread_mutex_unlock(&(pThreadLock->m));
}

void lockNotify(ThreadLock *pThreadLock) {
    pthread_mutex_lock(&(pThreadLock->m));
    pThreadLock->s = 1;
    pthread_cond_signal(&(pThreadLock->c));
    pthread_mutex_unlock(&(pThreadLock->m));
}

void lockDestroy(ThreadLock *pThreadLock) {
    lockNotify(pThreadLock);
    pthread_cond_destroy(&(pThreadLock->c));
    pthread_mutex_destroy(&(pThreadLock->m));
}

void openSLHelperInit(OpenSLHelper *pHelper) {
    pHelper->engineObject = NULL;
    pHelper->engineInterface = NULL;
    pHelper->recorderObject = NULL;
    pHelper->recorderInterface = NULL;
    pHelper->outputMixObject = NULL;
    pHelper->playerObject = NULL;
    pHelper->playInterface = NULL;

    //////Thread Lock//////
    lockInit(&(pHelper->threadLock));

    //////Engine Object//////
    slCreateEngine(&(pHelper->engineObject), 0, NULL, 0, NULL, NULL);
    (*pHelper->engineObject)->Realize(pHelper->engineObject, SL_BOOLEAN_FALSE);

    //////Engine Interface//////
    (*pHelper->engineObject)->GetInterface(
            pHelper->engineObject,
            SL_IID_ENGINE,
            &(pHelper->engineInterface)
    );
}

void openSLHelperDestroy(OpenSLHelper *pHelper) {
    //////Thread Lock//////
    lockDestroy(&(pHelper->threadLock));

    //////Player//////
    if (pHelper->playerObject) {
        (*pHelper->playerObject)->Destroy(pHelper->playerObject);
        pHelper->playerObject = NULL;
        pHelper->playInterface = NULL;
    }

    //////Recoder//////
    if (pHelper->recorderObject) {
        (*pHelper->recorderObject)->Destroy(pHelper->recorderObject);
        pHelper->recorderObject = NULL;
        pHelper->recorderInterface = NULL;
    }

    //////Outpute Mix//////
    if (pHelper->outputMixObject) {
        (*pHelper->outputMixObject)->Destroy(pHelper->outputMixObject);
        pHelper->outputMixObject = NULL;
    }

    //////Queue Interface//////
    if (pHelper->queueInterface) {
        pHelper->queueInterface = NULL;
    }

    //////Engine//////
    if (pHelper->engineObject) {
        (*pHelper->engineObject)->Destroy(pHelper->engineObject);
        pHelper->engineObject = NULL;
        pHelper->engineInterface = NULL;
    }
}

void recorderInit(OpenSLHelper *pHelper, SLuint32 numChannels, int samplingRate) {
    //////Source//////
    pHelper->device.locatorType = SL_DATALOCATOR_IODEVICE;
    pHelper->device.deviceType = SL_IODEVICE_AUDIOINPUT;
    pHelper->device.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT;
    pHelper->device.device = NULL; //Must be NULL if deviceID parameter is to be used.

    pHelper->source.pLocator = &(pHelper->device);
    pHelper->source.pFormat = NULL; //This parameter is ignored if pLocator is SLDataLocator_IODevice.

    //////Sink//////
    pHelper->queue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    pHelper->queue.numBuffers = 2;

    pHelper->format.formatType = SL_DATAFORMAT_PCM;
    pHelper->format.numChannels = numChannels;
    pHelper->format.samplesPerSec = convertSampleRate(samplingRate);
    pHelper->format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pHelper->format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    pHelper->format.channelMask = getChannelMask(numChannels);
    pHelper->format.endianness = SL_BYTEORDER_LITTLEENDIAN;

    pHelper->sink.pLocator = &(pHelper->queue);
    pHelper->sink.pFormat = &(pHelper->format);

    //////Recorder//////
    SLInterfaceID id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    SLboolean required[] = {SL_BOOLEAN_TRUE};

    (*pHelper->engineInterface)->CreateAudioRecorder(
            pHelper->engineInterface,
            &(pHelper->recorderObject),
            &(pHelper->source),
            &(pHelper->sink),
            1,
            id,
            required
    );
    (*pHelper->recorderObject)->Realize(pHelper->recorderObject, SL_BOOLEAN_FALSE);

    //////Register Callback//////
    (*pHelper->recorderObject)->GetInterface(
            pHelper->recorderObject,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &(pHelper->queueInterface)
    );

    (*pHelper->queueInterface)->RegisterCallback(
            pHelper->queueInterface,
            bufferQueueCallback,
            pHelper
    );

    //////Start Recording//////
    (*pHelper->recorderObject)->GetInterface(
            pHelper->recorderObject,
            SL_IID_RECORD,
            &(pHelper->recorderInterface)
    );

    (*pHelper->recorderInterface)->SetRecordState(
            pHelper->recorderInterface,
            SL_RECORDSTATE_RECORDING
    );
}

void playerInit(OpenSLHelper *pHelper, SLuint32 numChannels, int samplingRate) {
    //////Source//////
    pHelper->queue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    pHelper->queue.numBuffers = 2;

    pHelper->format.formatType = SL_DATAFORMAT_PCM;
    pHelper->format.numChannels = numChannels;
    pHelper->format.samplesPerSec = convertSampleRate(samplingRate);
    pHelper->format.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pHelper->format.containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    pHelper->format.channelMask = getChannelMask(numChannels);
    pHelper->format.endianness = SL_BYTEORDER_LITTLEENDIAN;

    pHelper->source.pLocator = &(pHelper->queue);
    pHelper->source.pFormat = &(pHelper->format);

    //////Sink//////
    (*pHelper->engineInterface)->CreateOutputMix(
            pHelper->engineInterface,
            &(pHelper->outputMixObject),
            0,
            NULL,
            NULL
    );
    (*pHelper->outputMixObject)->Realize(
            pHelper->outputMixObject,
            SL_BOOLEAN_FALSE
    );


    pHelper->outputMix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    pHelper->outputMix.outputMix = pHelper->outputMixObject;

    pHelper->sink.pLocator = &(pHelper->outputMix);
    pHelper->sink.pFormat = NULL; //This parameter is ignored if pLocator is SLDataLocator_IODevice or SLDataLocator_OutputMix.

    //////Player//////
    SLInterfaceID id[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    SLboolean required[] = {SL_BOOLEAN_TRUE};
    (*pHelper->engineInterface)->CreateAudioPlayer(
            pHelper->engineInterface,
            &(pHelper->playerObject),
            &(pHelper->source),
            &(pHelper->sink),
            1,
            id,
            required
    );
    (*pHelper->playerObject)->Realize(pHelper->playerObject, SL_BOOLEAN_FALSE);

    //////Register Callback//////
    (*pHelper->playerObject)->GetInterface(
            pHelper->playerObject,
            SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &(pHelper->queueInterface)
    );
    (*pHelper->queueInterface)->RegisterCallback(
            pHelper->queueInterface,
            bufferQueueCallback,
            pHelper
    );

    //////Begin Playing//////
    (*pHelper->playerObject)->GetInterface(
            pHelper->playerObject,
            SL_IID_PLAY,
            &(pHelper->playInterface)
    );
    (*pHelper->playInterface)->SetPlayState(
            pHelper->playInterface,
            SL_PLAYSTATE_PLAYING
    );
}