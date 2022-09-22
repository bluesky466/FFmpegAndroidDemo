#include "video_decoder.h"

#include <iostream>

extern "C" {
#include <libavutil/time.h>
}

using namespace std;

VideoDecoder::VideoDecoder() : 
        mFormatContext(NULL),
        mCodecContext(NULL),
        mPacket(NULL),
        mFrame(NULL),
        mVideoStreamIndex(-1),
        mVideoWidth(-1),
        mVideoHegiht(-1),
        mDecodecStart(-1),
        mLastDecodecTime(-1),
        mPixelFormat(AV_PIX_FMT_NONE) {
}

bool VideoDecoder::Load(const string& url) {
    mUrl = url;

    // 打开文件流读取文件头解析出视频信息如轨道信息、时长等
    // mFormatContext初始化为NULL,如果打开成功,它会被设置成非NULL的值
    // 这个方法实际可以打开多种来源的数据,url可以是本地路径、rtmp地址等
    // 在不需要的时候通过avformat_close_input关闭文件流
    if(avformat_open_input(&mFormatContext, url.c_str(), NULL, NULL) < 0) {
        cout << "open " << url << " failed" << endl;
        return false;
    }

    // 对于没有文件头的格式如MPEG或者H264裸流等,可以通过这个函数解析前几帧得到视频的信息
    if(avformat_find_stream_info(mFormatContext, NULL) < 0) {
        cout << "can't find stream info in " << url << endl;
        return false;
    }

    // 查找视频轨道,实际上我们也可以通过遍历AVFormatContext的streams得到,代码如下:
    // for(int i = 0 ; i < mFormatContext->nb_streams ; i++) {
    //     if(mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
    //         mVideoStreamIndex = i;
    //         break;
    //     }
    // }
    mVideoStreamIndex = av_find_best_stream(mFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if(mVideoStreamIndex < 0) {
        cout << "can't find video stream in " << url << endl;
        return false;
    }

    // 获取视频轨道的解码器相关参数
    AVCodecParameters* codecParam = mFormatContext->streams[mVideoStreamIndex]->codecpar;
    cout << "codec id = " << codecParam->codec_id << endl;
    
    // 通过codec_id获取到对应的解码器
    // codec_id是enum AVCodecID类型,我们可以通过它知道视频流的格式,如AV_CODEC_ID_H264(0x1B)、AV_CODEC_ID_H265(0xAD)等
    // 当然如果是音频轨道的话它的值可能是AV_CODEC_ID_MP3(0x15001)、AV_CODEC_ID_AAC(0x15002)等
    AVCodec* codec = avcodec_find_decoder(codecParam->codec_id);
    if(codec == NULL) {
        cout << "can't find codec" << endl;
        return false;
    }

    // 创建解码器上下文,解码器的一些环境就保存在这里
    // 在不需要的时候可以通过avcodec_free_context释放
    mCodecContext = avcodec_alloc_context3(codec);
    if (mCodecContext == NULL) {
        cout << "can't alloc codec context" << endl;
        return false;
    }


    // 设置解码器参数
    if(avcodec_parameters_to_context(mCodecContext, codecParam) < 0) {
        cout << "can't set codec params" << endl;
        return false;
    }

    // 打开解码器,从源码里面看到在avcodec_free_context释放解码器上下文的时候会close,
    // 所以我们可以不用自己调用avcodec_close去关闭
    if(avcodec_open2(mCodecContext, codec, NULL) < 0) {
        cout << "can't open codec" << endl;
        return false;
    }

    // 创建创建AVPacket接收数据包
    // 无论是压缩的音频流还是压缩的视频流,都是由一个个数据包组成的
    // 解码的过程实际就是从文件流中读取一个个数据包传给解码器去解码
    // 对于视频，它通常应包含一个压缩帧
    // 对于音频，它可能是一段压缩音频、包含多个压缩帧
    // 在不需要的时候可以通过av_packet_free释放
    mPacket = av_packet_alloc();
    if(NULL == mPacket) {
        cout << "can't alloc packet" << endl;
        return false;
    }

    // 创建AVFrame接收解码器解码出来的原始数据(视频的画面帧或者音频的PCM裸流)
    // 在不需要的时候可以通过av_frame_free释放
    mFrame = av_frame_alloc();
    if(NULL == mFrame) {
        cout << "can't alloc frame" << endl;
        return false;
    }

    // 可以从解码器上下文获取视频的尺寸
    // 这个尺寸实际上是从AVCodecParameters里面复制过去的,所以直接用codecParam->width、codecParam->height也可以
    mVideoWidth = mCodecContext->width;
    mVideoHegiht =  mCodecContext->height;

    // 可以从解码器上下文获取视频的像素格式
    // 这个像素格式实际上是从AVCodecParameters里面复制过去的,所以直接用codecParam->format也可以
    mPixelFormat = mCodecContext->pix_fmt;

    return true;
}

AVPixelFormat VideoDecoder::GetPixelFormat() {
    return mPixelFormat;
}

void VideoDecoder::Release() {
    mUrl = "";
    mVideoStreamIndex = -1;
    mVideoWidth = -1;
    mVideoHegiht = -1;
    mDecodecStart = -1;
    mLastDecodecTime = -1;
    mPixelFormat = AV_PIX_FMT_NONE;

    if(NULL != mFormatContext) {
        avformat_close_input(&mFormatContext);
    }

    if (NULL != mCodecContext) {
        avcodec_free_context(&mCodecContext);
    }
    
    if(NULL != mPacket) {
        av_packet_free(&mPacket);
    }

    if(NULL != mFrame) {
        av_frame_free(&mFrame);
    }
}

AVFrame* VideoDecoder::NextFrame() {
    // 从文件流里面读取出数据包,这里的数据包是编解码层的压缩数据
    if(av_read_frame(mFormatContext, mPacket) < 0) {
        return NULL;
    }

    AVFrame* frame = NULL;

    // 这里的数据包可能是各个流的包,所以需要通过stream_index过滤出我们需要的视频流
    // 然后通过avcodec_send_packet将数据包发送给解码器
    // 接着通过avcodec_receive_frame读取到解码器解出来的原始像素数据
    // avcodec_receive_frame内会调用av_frame_unref将上一帧的内存清除,而最后一帧的数据也会在Release的时候被av_frame_free清除
    // 所以不需要手动调用av_frame_unref
    if(mPacket->stream_index == mVideoStreamIndex
        && avcodec_send_packet(mCodecContext, mPacket) == 0
        && avcodec_receive_frame(mCodecContext, mFrame) == 0) {
        frame = mFrame;

        // 由于解码的速度比较快,我们可以等到需要播放的时候再去解码下一帧
        // 这样可以降低cpu的占用,也能减少绘制线程堆积画面队列造成内存占用过高
        // 由于这个demo没有单独的解码线程,在渲染线程进行解码,sdl渲染本身就耗时
        // 所以就算不延迟也会发现画面是正常速度播放的
        // 可以将绘制的代码注释掉,然后在该方法内加上打印,会发现一下子就解码完整个视频了
        if(AV_NOPTS_VALUE == mFrame->pts) {
            // 有些视频流不带pts数据,按30fps将每帧间隔统一成32ms
            int64_t sleep = 32000 - (av_gettime() - mLastDecodecTime);
            if(mLastDecodecTime != -1 && sleep > 0) {
                av_usleep(sleep);
            }
            mLastDecodecTime = av_gettime();
        } else {
            // 带pts数据的视频流,我们计算出这一帧应该在什么时候播放,如果时间还没有到就添加延迟

            // time_base即pts的单位,AVRational是个分数,代表几分之几秒
            AVRational timebase = mFormatContext->streams[mPacket->stream_index]->time_base;

            // 我们用timebase.num * 1.0f / timebase.den计算出这个分数的值,然后*1000等到ms,再*1000得到us
            // 后半部分的计算其实可以放到VideoDecoder::Load里面保存到成员变量,但是为了讲解方便就放在这里了
            int64_t pts = mFrame->pts * 1000 * 1000 * timebase.num * 1.0f / timebase.den;


            // 如果是第一帧就记录开始时间
            if(-1 == mDecodecStart) {
                mDecodecStart = av_gettime() - pts;
            }

            // 当前时间减去开始时间,得到当前播放到了视频的第几微秒
            int64_t now = av_gettime() - mDecodecStart;

            // 如果这一帧的播放时间还没有到就等到播放时间到了再返回
            if(pts > now) {
                av_usleep(pts - now);
            }
        }
    }

    // 解码完成之后压缩数据包的数据就不需要了,将它释放
    av_packet_unref(mPacket);

    // 由于视频压缩帧存在i帧、b帧、p帧这些类型,并不是每种帧都可以直接解码出原始画面
    // b帧是双向差别帧，也就是说b帧记录的是本帧与前后帧的差别,还需要后面的帧才能解码
    // 如果这一帧AVPacket没有解码出数据来的话,就递归调用NextFrame解码下一帧,直到解出下一帧原生画面来
    if(frame == NULL) {
        return NextFrame();
    }

    return frame;
}

void VideoDecoder::DumpVideoInfo() {
    av_dump_format(mFormatContext, 0, mUrl.c_str(), 0);
}

int VideoDecoder::GetVideoWidth() {
    return mVideoWidth;
}

int VideoDecoder::GetVideoHeight() {
    return mVideoHegiht;
}