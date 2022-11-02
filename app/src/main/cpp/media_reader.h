#ifndef __VIDEO_DECODER_H__
#define __VIDEO_DECODER_H__

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include "stream_decoder.h"

extern "C" {
#include <libavformat/avformat.h>
}

class MediaReader {
public:
    MediaReader();

    bool Load(const std::string &url);

    void Release();

    void DumpVideoInfo() const;

    std::vector<AVStream *> GetStreams() const;

    AVStream *GetStream(int index) const;

    void SetStreamEnable(int streamIndex);

    bool IsStreamEnable(int streamIndex) const;

    AVPacket *NextPacket(int streamIndex);

    int64_t GetReadStartTime() const;
private:
    AVFormatContext *mFormatContext;
    std::vector<std::queue<AVPacket *>> mPackets;
    u_int64_t mStreamEnableFlags;
    std::mutex mMutex;
    std::string mUrl;
    int64_t mReadStart;
};

#endif