#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class VideoDecoder {
public:
    VideoDecoder();
    bool Load(const std::string& url);
    void Release();

    AVFrame* NextFrame();

    void DumpVideoInfo();
    int GetVideoWidth();
    int GetVideoHeight();
    AVPixelFormat GetPixelFormat();

private:
    AVFormatContext* mFormatContext;
    AVCodecContext* mCodecContext;
    AVPacket* mPacket;
    AVFrame* mFrame;

    std::string mUrl;
    int mVideoStreamIndex;
    int mVideoWidth;
    int mVideoHegiht;
    int64_t mDecodecStart;
    int64_t mLastDecodecTime;
    AVPixelFormat mPixelFormat;
};