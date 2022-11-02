#include <iostream>

extern "C" {
#include <libavutil/time.h>
}

#include "media_reader.h"
#include "common.h"

using namespace std;

MediaReader::MediaReader() :
        mFormatContext(NULL),
        mReadStart(-1),
        mStreamEnableFlags(0) {
}

bool MediaReader::Load(const string &url) {
    mUrl = url;

    // 打开文件流读取文件头解析出视频信息如轨道信息、时长等
    // mFormatContext初始化为NULL,如果打开成功,它会被设置成非NULL的值
    // 这个方法实际可以打开多种来源的数据,url可以是本地路径、rtmp地址等
    // 在不需要的时候通过avformat_close_input关闭文件流
    if (avformat_open_input(&mFormatContext, url.c_str(), NULL, NULL) < 0) {
        LOGD("open %s failed", url.c_str());
        return false;
    }

    // 对于没有文件头的格式如MPEG或者H264裸流等,可以通过这个函数解析前几帧得到视频的信息
    if (avformat_find_stream_info(mFormatContext, NULL) < 0) {
        LOGD("can't find stream info in ");
        return false;
    }

    mPackets.resize(mFormatContext->nb_streams, queue<AVPacket *>());

    return true;
}

void MediaReader::Release() {
    mUrl = "";
    mReadStart = -1;
    mStreamEnableFlags = 0;

    if (NULL != mFormatContext) {
        avformat_close_input(&mFormatContext);
    }
}

void MediaReader::DumpVideoInfo() const {
    av_dump_format(mFormatContext, 0, mUrl.c_str(), 0);
}

void MediaReader::SetStreamEnable(int streamIndex) {
    if (streamIndex >= mFormatContext->nb_streams) {
        LOGD("SetStreamEnable false, streamIndex >= mFormatContext->nb_streams %d,%d",
             streamIndex, mFormatContext->nb_streams);
        return;
    }
    mStreamEnableFlags |= (1 << streamIndex);
}

inline bool MediaReader::IsStreamEnable(int streamIndex) const {
    return mStreamEnableFlags & (1 << streamIndex);
}

std::vector<AVStream *> MediaReader::GetStreams() const {
    vector<AVStream *> streams;

    if (mFormatContext != NULL) {
        streams.resize(mFormatContext->nb_streams, NULL);
        for (int i = 0; i < mFormatContext->nb_streams; i++) {
            streams[i] = mFormatContext->streams[i];
        }
    }
    return streams;
}

AVPacket *MediaReader::NextPacket(int streamIndex) {
    lock_guard<mutex> lock(mMutex);

    if (!IsStreamEnable(streamIndex) || streamIndex >= mPackets.size()) {
        return NULL;
    }

    LOGD("NextPacket %d start", streamIndex);
    // 如果是第一帧就记录开始时间
    if (-1 == mReadStart) {
        mReadStart = av_gettime();
    }

    AVPacket *packet = NULL;
    queue<AVPacket *> &queue = mPackets[streamIndex];
    if (!queue.empty()) {
        packet = queue.front();
        queue.pop();
        LOGD("NextPacket %d finish", streamIndex);
        return packet;
    }

    packet = av_packet_alloc();
    while (av_read_frame(mFormatContext, packet) == 0) {
        if (packet->stream_index == streamIndex) {
            LOGD("NextPacket %d finish", streamIndex);
            return packet;
        } else if (IsStreamEnable(packet->stream_index)) {
            mPackets[packet->stream_index].push(packet);
            packet = av_packet_alloc();
        } else {
            av_packet_unref(packet);
        }
    }
    av_packet_free(&packet);
    return NULL;
}

int64_t MediaReader::GetReadStartTime() const {
    return mReadStart;
}

AVStream *MediaReader::GetStream(int index) const {
    return mFormatContext->streams[index];
}
