#ifndef __STREAM_READER_H__
#define __STREAM_READER_H__


extern "C" {
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

class MediaReader;

class StreamDecoder {
public:
    StreamDecoder();

    virtual bool Init(MediaReader *reader, int streamIndex);

    virtual void Destroy();

    virtual AVFrame *NextFrame();

protected:
    MediaReader *mReader;
    int mStreamIndex;
    AVFrame *mFrame;

    int64_t mLastDecodeTime;
    AVCodecContext *mCodecContext;
};

class VideoStreamDecoder : public StreamDecoder {
public:
    VideoStreamDecoder();

    virtual bool Init(MediaReader *reader, int streamIndex);

    bool Init(MediaReader *reader, int streamIndex, AVPixelFormat pixelFormat);

    virtual void Destroy();

    virtual AVFrame *NextFrame();

    int GetVideoWidth() const;

    int GetVideoHeight() const;

    AVPixelFormat GetPixelFormat() const;

private:
    SwsContext *mSwsContext;
    AVFrame *mSwrFrame;
    AVPixelFormat mPixelFormat;
};

class AudioStreamDecoder : public StreamDecoder {
public:
    AudioStreamDecoder();

    virtual bool Init(MediaReader *reader, int streamIndex);

    bool Init(MediaReader *reader, int streamIndex, AVSampleFormat sampleFormat);

    virtual void Destroy();

    virtual AVFrame *NextFrame();

    int GetSampleRate() const;

    int GetChannelCount() const;

    int GetBytePerSample() const;

    AVSampleFormat GetSampleFormat() const;

private:
    SwrContext *mSwrContext;
    AVFrame *mSwrFrame;
    AVSampleFormat mSampleFormat;
};

#endif