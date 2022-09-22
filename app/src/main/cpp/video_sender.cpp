#include "video_sender.h"

#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/time.h>
}

using namespace std;

static bool createOutputStreams(AVFormatContext* inputFormatContext, AVFormatContext* outputFormatContext) {
    // 遍历输入流的所有轨道,拷贝编解码参数到输出流
    for(int i = 0 ; i < inputFormatContext->nb_streams ; i++) {
        // 为输出流创建轨道
        AVStream* stream = avformat_new_stream(outputFormatContext, NULL);
        if(NULL == stream) {
            cout << "can't create stream, index " << i << endl;
            return false;
        }

        // 编解码参数在AVCodecParameters中保存,从输入流拷贝到输出流
        if(avcodec_parameters_copy(stream->codecpar, inputFormatContext->streams[i]->codecpar) < 0) {
            cout << "can't copy codec paramters, stream index " << i << endl;
            return false;
        }

        // codec_tag代表了音视频数据采用的码流格式,不同的封装格式如flv、mp4等的支持情况是不一样的
        // 上面的avcodec_parameters_copy将输出流的codec_tag从输入拷贝过来变成了一样的
        // 由于我们输出流在avformat_alloc_output_context2的时候写死了flv格式
        // 如果输入流不是flv而是mp4等格式的话就可能会出现mp4里某种codec_tag在flv不支持导致推流失败的情况
        // 这里我们可以用av_codec_get_id从输出流的oformat的支持的codec_tag列表里面查找codec_id
        // 如果和codecpar的codec_id不一致的话代表不支持
        if(av_codec_get_id(outputFormatContext->oformat->codec_tag, stream->codecpar->codec_tag) != stream->codecpar->codec_id) {
            // 这里将codec_tag设置为0,FFmpeg会根据编码codec_id从封装格式的codec_tag列表中找到一个codec_tag
            stream->codecpar->codec_tag = 0;
        }
    }
    return true;
}

bool VideoSender::Send(const string& srcUrl, const string& destUrl) {
    bool result = false;
    AVFormatContext* inputFormatContext = NULL;
    AVFormatContext* outputFormatContext = NULL;
    AVPacket* packet = NULL;

    do {
        // 打开文件流读取文件头解析出视频信息如轨道信息、时长等
        // mFormatContext初始化为NULL,如果打开成功,它会被设置成非NULL的值
        // 这个方法实际可以打开多种来源的数据,url可以是本地路径、rtmp地址等
        // 在不需要的时候通过avformat_close_input关闭文件流
        if(avformat_open_input(&inputFormatContext, srcUrl.c_str(), NULL, NULL) < 0) {
            cout << "open " << srcUrl << " failed" << endl;
            break;
        }

        // 对于没有文件头的格式如MPEG或者H264裸流等,可以通过这个函数解析前几帧得到视频的信息
        if(avformat_find_stream_info(inputFormatContext, NULL) < 0) {
            cout << "can't find stream info in " << srcUrl << endl;
            break;
        }

        // 打印输入视频信息
        av_dump_format(inputFormatContext, 0, srcUrl.c_str(), 0);

        // 创建输出流上下文,outputFormatContext初始化为NULL,如果打开成功,它会被设置成非NULL的值,在不需要的时候使用avformat_free_context释放
        // 输出流使用flv格式
        if(avformat_alloc_output_context2(&outputFormatContext, NULL, "flv", destUrl.c_str()) < 0) {
            cout << "can't alloc output context for " << destUrl << endl;
            break;
        }

        // 拷贝编解码参数
        if(!createOutputStreams(inputFormatContext, outputFormatContext)) {
            break;
        }

        // 打印输出视频信息
        av_dump_format(outputFormatContext, 0, destUrl.c_str(), 1);

        // 打开输出流,结束的时候使用avio_close关闭
        if(avio_open(&outputFormatContext->pb, destUrl.c_str(), AVIO_FLAG_WRITE) < 0) {
            cout << "can't open avio " << destUrl << endl;
            break;
        }

        // 设置flvflags为no_duration_filesize用于解决下面的报错
        // [flv @ 0x14f808e00] Failed to update header with correct duration.
        // [flv @ 0x14f808e00] Failed to update header with correct filesize
        AVDictionary * opts = NULL;
        av_dict_set(&opts, "flvflags", "no_duration_filesize", 0);
        if(avformat_write_header(outputFormatContext, opts ? &opts : NULL) < 0) {
            cout << "write header to " << destUrl << " failed" << endl;
            break;
        }

        // 创建创建AVPacket接收数据包
        // 无论是压缩的音频流还是压缩的视频流,都是由一个个数据包组成的
        // 解码的过程实际就是从文件流中读取一个个数据包传给解码器去解码
        // 对于视频，它通常应包含一个压缩帧
        // 对于音频，它可能是一段压缩音频、包含多个压缩帧
        // 在不需要的时候可以通过av_packet_free释放
        packet = av_packet_alloc();
        if(NULL == packet) {
            cout << "can't alloc packet" << endl;
            break;
        }

        // 查找视频轨道,实际上我们也可以通过遍历AVFormatContext的streams得到,代码如下:
        // for(int i = 0 ; i < mFormatContext->nb_streams ; i++) {
        //     if(mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        //         mVideoStreamIndex = i;
        //         break;
        //     }
        // }
        const int videoStreamIndex = av_find_best_stream(inputFormatContext, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        
        // time_base即pts的单位,AVRational是个分数,代表几分之几秒
        AVRational timeBase = inputFormatContext->streams[videoStreamIndex]->time_base;
        const float timeBaseFloat = timeBase.num * 1.0 / timeBase.den;

        //推流开始时间
        int64_t startTime = av_gettime();

        // 从文件流里面读取出数据包,这里的数据包是编解码层的压缩数据
        while(av_read_frame(inputFormatContext, packet) >= 0) {
            // 我们以视频轨道为基准去同步时间
            // 如果时间还没有到就添加延迟,避免向服务器推流速度过快
            if(videoStreamIndex == packet->stream_index) {
                if(AV_NOPTS_VALUE == packet->pts) {
                    // 有些视频流不带pts数据,按30fps将间隔统一成32ms
                    av_usleep(32000);
                } else {
                    // 带pts数据的视频流,我们计算出每一帧应该在什么时候播放
                    int64_t nowTime = av_gettime() - startTime;
                    int64_t pts = packet->pts * 1000 * 1000 * timeBaseFloat;
                    if(pts > nowTime) {
                        av_usleep(pts - nowTime);
                    }
                }
            }
            // 往输出流写入数据
            av_interleaved_write_frame(outputFormatContext, packet);

            // 写入成之后压缩数据包的数据就不需要了,将它释放
            av_packet_unref(packet);
        }

        // 写入视频尾部信息
        av_write_trailer(outputFormatContext);

        result = true;
    } while(0);

    if(NULL != packet) {
        av_packet_free(&packet);
    }
    
    if(NULL != outputFormatContext) {
        if(NULL != outputFormatContext->pb) {
            avio_close(outputFormatContext->pb);
        }
        avformat_free_context(outputFormatContext);
    }

    if(NULL != inputFormatContext) {
        avformat_close_input(&inputFormatContext);
    }

    return result;
}