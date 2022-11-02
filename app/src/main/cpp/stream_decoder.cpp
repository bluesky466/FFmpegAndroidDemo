#include <iostream>

extern "C" {
#include <libavutil/time.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
}

#include "stream_decoder.h"
#include "media_reader.h"
#include "common.h"

using namespace std;

bool StreamDecoder::Init(MediaReader *reader, int streamIndex) {
    mReader = reader;
    mStreamIndex = streamIndex;

    // 获取视频轨道的解码器相关参数
    AVCodecParameters *codecParam = reader->GetStream(streamIndex)->codecpar;
    LOGD("codec id = %d", codecParam->codec_id);

    // 通过codec_id获取到对应的解码器
    // codec_id是enum AVCodecID类型,我们可以通过它知道视频流的格式,如AV_CODEC_ID_H264(0x1B)、AV_CODEC_ID_H265(0xAD)等
    // 当然如果是音频轨道的话它的值可能是AV_CODEC_ID_MP3(0x15001)、AV_CODEC_ID_AAC(0x15002)等
    AVCodec *codec = avcodec_find_decoder(codecParam->codec_id);
    if (codec == NULL) {
        LOGD("can't find codec");
        return false;
    }

    // 创建解码器上下文,解码器的一些环境就保存在这里
    // 在不需要的时候可以通过avcodec_free_context释放
    mCodecContext = avcodec_alloc_context3(codec);
    if (mCodecContext == NULL) {
        LOGD("can't alloc codec context");
        return false;
    }

    // 设置解码器参数
    if (avcodec_parameters_to_context(mCodecContext, codecParam) < 0) {
        LOGD("can't set codec params");
        return false;
    }

    // 打开解码器,从源码里面看到在avcodec_free_context释放解码器上下文的时候会close,
    // 所以我们可以不用自己调用avcodec_close去关闭
    if (avcodec_open2(mCodecContext, codec, NULL) < 0) {
        LOGD("can't open codec");
        return false;
    }

    // 创建AVFrame接收解码器解码出来的原始数据(视频的画面帧或者音频的PCM裸流)
    // 在不需要的时候可以通过av_frame_free释放
    mFrame = av_frame_alloc();
    if (NULL == mFrame) {
        LOGD("can't alloc frame");
        return false;
    }

    mReader->SetStreamEnable(streamIndex);
    return true;
}

AVFrame *StreamDecoder::NextFrame() {
    AVPacket *packet = mReader->NextPacket(mStreamIndex);
    if (NULL == packet) {
        return NULL;
    }

    AVFrame *frame = NULL;

    // 这里的数据包可能是各个流的包,所以需要通过stream_index过滤出我们需要的视频流
    // 然后通过avcodec_send_packet将数据包发送给解码器
    // 接着通过avcodec_receive_frame读取到解码器解出来的原始像素数据
    // avcodec_receive_frame内会调用av_frame_unref将上一帧的内存清除,而最后一帧的数据也会在Release的时候被av_frame_free清除
    // 所以不需要手动调用av_frame_unref
    if (avcodec_send_packet(mCodecContext, packet) == 0
        && avcodec_receive_frame(mCodecContext, mFrame) == 0) {
        frame = mFrame;
        // 由于解码的速度比较快,我们可以等到需要播放的时候再去解码下一帧
        // 这样可以降低cpu的占用,也能减少绘制线程堆积画面队列造成内存占用过高
        // 由于这个demo没有单独的解码线程,在渲染线程进行解码,sdl渲染本身就耗时
        // 所以就算不延迟也会发现画面是正常速度播放的
        // 可以将绘制的代码注释掉,然后在该方法内加上打印,会发现一下子就解码完整个视频了
        if (AV_NOPTS_VALUE == mFrame->pts) {
            // 有些视频流不带pts数据,按30fps将每帧间隔统一成32ms
            int64_t sleep = 32000 - (av_gettime() - mLastDecodeTime);
            if (mLastDecodeTime != -1 && sleep > 0) {
                av_usleep(sleep);
            }
            mLastDecodeTime = av_gettime();
        } else {
            // 带pts数据的视频流,我们计算出这一帧应该在什么时候播放,如果时间还没有到就添加延迟

            // time_base即pts的单位,AVRational是个分数,代表几分之几秒
            AVRational timebase = mReader->GetStream(packet->stream_index)->time_base;

            // 我们用timebase.num * 1.0f / timebase.den计算出这个分数的值,然后*1000等到ms,再*1000得到us
            // 后半部分的计算其实可以放到VideoDecoder::Load里面保存到成员变量,但是为了讲解方便就放在这里了
            int64_t pts = mFrame->pts * 1000 * 1000 * timebase.num * 1.0f / timebase.den;

            // 当前时间减去开始时间,得到当前播放到了视频的第几微秒
            int64_t now = av_gettime() - mReader->GetReadStartTime();

            // 如果这一帧的播放时间还没有到就等到播放时间到了再返回
            if (pts > now) {
                av_usleep(pts - now);
            }
        }
    }

    // 解码完成之后压缩数据包的数据就不需要了,将它释放
    av_packet_free(&packet);

    // 由于视频压缩帧存在i帧、b帧、p帧这些类型,并不是每种帧都可以直接解码出原始画面
    // b帧是双向差别帧，也就是说b帧记录的是本帧与前后帧的差别,还需要后面的帧才能解码
    // 如果这一帧AVPacket没有解码出数据来的话,就递归调用NextFrame解码下一帧,直到解出下一帧原生画面来
    if (frame == NULL) {
        return NextFrame();
    }

    return frame;
}

StreamDecoder::StreamDecoder() :
        mFrame(NULL),
        mCodecContext(NULL) {
}

void StreamDecoder::Destroy() {
    if (NULL != mCodecContext) {
        avcodec_free_context(&mCodecContext);
    }
    if (NULL != mFrame) {
        av_frame_free(&mFrame);
    }
}

bool VideoStreamDecoder::Init(MediaReader *reader, int streamIndex, AVPixelFormat pixelFormat) {
    AVMediaType type = reader->GetStream(streamIndex)->codecpar->codec_type;
    if (AVMEDIA_TYPE_VIDEO != type) {
        LOGD("VideoStreamDecoder not support streamIndex %d of type %d", streamIndex, type);
        return false;
    }
    bool result = StreamDecoder::Init(reader, streamIndex);
    if (AVPixelFormat::AV_PIX_FMT_NONE == pixelFormat) {
        mPixelFormat = mCodecContext->pix_fmt;
    } else {
        mPixelFormat = pixelFormat;
    }

    if (mPixelFormat != mCodecContext->pix_fmt) {
        int width = mCodecContext->width;
        int height = mCodecContext->height;

        mSwrFrame = av_frame_alloc();

        // 方式一,使用av_frame_get_buffer创建数据存储空间,av_frame_free的时候会自动释放
        mSwrFrame->width = width;
        mSwrFrame->height = height;
        mSwrFrame->format = mPixelFormat;
        av_frame_get_buffer(mSwrFrame, 0);

        // 方式二,使用av_image_fill_arrays指定存储空间,需要我们手动调用av_malloc、av_free去创建、释放空间
//        int bufferSize = av_image_get_buffer_size(mPixelFormat, width, height, 16);
//        unsigned char* buffer = (unsigned char *)av_malloc(bufferSize);
//        av_image_fill_arrays(mSwrFrame->data, mSwrFrame->linesize, buffer, mPixelFormat, width, height, 16);

        mSwsContext = sws_getContext(
                mCodecContext->width, mCodecContext->height, mCodecContext->pix_fmt,
                width, height, mPixelFormat, SWS_BICUBIC,
                NULL, NULL, NULL
        );
    }
    return result;
}

bool VideoStreamDecoder::Init(MediaReader *reader, int streamIndex) {
    return Init(reader, streamIndex, AVPixelFormat::AV_PIX_FMT_NONE);
}

int VideoStreamDecoder::GetVideoWidth() const {
    if (NULL == mCodecContext) {
        return -1;
    }
    return mCodecContext->width;
}

int VideoStreamDecoder::GetVideoHeight() const {
    if (NULL == mCodecContext) {
        return -1;
    }
    return mCodecContext->height;
}

AVPixelFormat VideoStreamDecoder::GetPixelFormat() const {
    return mPixelFormat;
}

void VideoStreamDecoder::Destroy() {
    StreamDecoder::Destroy();
    if (NULL != mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = NULL;
    }
    if (NULL != mSwrFrame) {
        av_frame_free(&mSwrFrame);
    }
}

VideoStreamDecoder::VideoStreamDecoder()
        : mSwsContext(NULL),
          mSwrFrame(NULL),
          mPixelFormat(AVPixelFormat::AV_PIX_FMT_NONE) {

}

AVFrame *VideoStreamDecoder::NextFrame() {
    AVFrame *frame = StreamDecoder::NextFrame();
    if (NULL == frame) {
        return NULL;
    }
    if (NULL == mSwsContext) {
        return frame;
    }
    sws_scale(mSwsContext, frame->data,
              frame->linesize, 0, mCodecContext->height,
              mSwrFrame->data, mSwrFrame->linesize);
    return mSwrFrame;
}

bool AudioStreamDecoder::Init(MediaReader *reader, int streamIndex) {
    return AudioStreamDecoder::Init(reader, streamIndex, AVSampleFormat::AV_SAMPLE_FMT_NONE);
}

bool AudioStreamDecoder::Init(MediaReader *reader, int streamIndex, AVSampleFormat sampleFormat) {
    AVMediaType type = reader->GetStream(streamIndex)->codecpar->codec_type;
    if (AVMEDIA_TYPE_AUDIO != type) {
        LOGD("AudioStreamDecoder not support streamIndex %d of type %d", streamIndex, type);
        return false;
    }

    bool result = StreamDecoder::Init(reader, streamIndex);

    if (sampleFormat == AVSampleFormat::AV_SAMPLE_FMT_NONE) {
        mSampleFormat = mCodecContext->sample_fmt;
    } else {
        mSampleFormat = sampleFormat;
    }

    if (mSampleFormat != mCodecContext->sample_fmt) {
        mSwrContext = swr_alloc_set_opts(
                NULL,
                mCodecContext->channel_layout,
                mSampleFormat,
                mCodecContext->sample_rate,
                mCodecContext->channel_layout,
                mCodecContext->sample_fmt,
                mCodecContext->sample_rate,
                0,
                NULL);
        swr_init(mSwrContext);

        // 虽然前面的swr_alloc_set_opts已经设置了这几个参数
        // 但是用于接收的AVFrame不设置这几个参数也会接收不到数据
        // 原因是后面的swr_convert_frame函数会通过av_frame_get_buffer创建数据的buff
        // 而av_frame_get_buffer需要AVFrame设置好这些参数去计算buff的大小
        mSwrFrame = av_frame_alloc();
        mSwrFrame->channel_layout = mCodecContext->channel_layout;
        mSwrFrame->sample_rate = mCodecContext->sample_rate;
        mSwrFrame->format = mSampleFormat;
    }
    return result;
}

void AudioStreamDecoder::Destroy() {
    StreamDecoder::Destroy();

    if (NULL != mSwrContext) {
        swr_free(&mSwrContext);
    }
    if (NULL != mSwrFrame) {
        av_frame_free(&mSwrFrame);
    }
}

int AudioStreamDecoder::GetSampleRate() const {
    if (NULL == mCodecContext) {
        return -1;
    }
    return mCodecContext->sample_rate;
}

int AudioStreamDecoder::GetChannelCount() const {
    if (NULL == mCodecContext) {
        return -1;
    }
    return mCodecContext->channels;
}

AudioStreamDecoder::AudioStreamDecoder()
        : mSwrContext(NULL),
          mSwrFrame(NULL),
          mSampleFormat(AVSampleFormat::AV_SAMPLE_FMT_NONE) {
}

AVFrame *AudioStreamDecoder::NextFrame() {
    AVFrame *frame = StreamDecoder::NextFrame();
    if (NULL == frame) {
        return NULL;
    }
    if (NULL == mSwrContext) {
        return frame;
    }

    swr_convert_frame(mSwrContext, mSwrFrame, frame);
    return mSwrFrame;
}

int AudioStreamDecoder::GetBytePerSample() const {
    return av_get_bytes_per_sample(mSampleFormat);
}

AVSampleFormat AudioStreamDecoder::GetSampleFormat() const {
    return mSampleFormat;
}
