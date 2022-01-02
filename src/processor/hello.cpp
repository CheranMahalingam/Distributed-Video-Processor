extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <fstream>
#include <iostream>
#include <string>

#include "log.h"

static int decode_packet(AVPacket* packet, AVCodecContext* codec_ctx, AVFrame* frame);

static void save_grey_frame(unsigned char* buffer, int wrap, int x_size, int y_size, char* filename);

int main() {
    AVFormatContext* format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        logger(LogLevel::Error) << "Could not allocate memory for format_ctx";
        return -1;
    }

    if (avformat_open_input(&format_ctx, "../../src/video/bunny.mp4", NULL, NULL) != 0) {
        logger(LogLevel::Error) << "Could not open file";
        return -1;
    }

    logger(LogLevel::Debug) << "format:" << format_ctx->iformat->name << "duration:" << format_ctx->duration << "bitrate:" << format_ctx->bit_rate;

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        logger(LogLevel::Error) << "Could not get stream info";
        return -1;
    }

    AVCodec* codec;
    AVCodecParameters* codec_params;
    int stream_index = -1;

    for (int i = 0; i < format_ctx->nb_streams; i++) {
        auto* local_params = format_ctx->streams[i]->codecpar;
        logger(LogLevel::Debug) << "time_base before open coded" << format_ctx->streams[i]->time_base.num << "/" << format_ctx->streams[i]->time_base.den;
        logger(LogLevel::Debug) << "r_frame_rate before open coded" << format_ctx->streams[i]->r_frame_rate.num << "/" << format_ctx->streams[i]->r_frame_rate.den;
        logger(LogLevel::Debug) << "start_time" << format_ctx->streams[i]->start_time;
        logger(LogLevel::Debug) << "duration" << format_ctx->streams[i]->duration;

        auto* local_codec = avcodec_find_decoder(local_params->codec_id);
        if (local_codec == NULL) {
            logger(LogLevel::Error) << "Unsupported codec";
            continue;
        }

        if (local_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (stream_index == -1) {
                stream_index = i;
                codec = local_codec;
                codec_params = local_params;
            }

            logger(LogLevel::Debug) << "Video Codec: resolution" << local_params->width << "x" << local_params->height;
        } else if (local_params->codec_type == AVMEDIA_TYPE_AUDIO) {
            logger(LogLevel::Debug) << "Audio Codec:" << local_params->channels << "channels, sample rate" << local_params->sample_rate; 
        }

        logger(LogLevel::Debug) << "Codec" << local_codec->name << "ID" << local_codec->id << "bit_rate" << local_params->bit_rate;
    }

    if (stream_index == -1) {
        logger(LogLevel::Error) << "File does not contain a video stream";
        return -1;
    }

    auto* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        logger(LogLevel::Error) << "Failed to allocate memory for AVCodecContext";
        return -1;
    }

    if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
        logger(LogLevel::Error) << "Failed to copy codec params to codec context";
        return -1;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        logger(LogLevel::Error) << "Failed to open codec";
        return -1;
    }

    auto* frame = av_frame_alloc();
    if (!frame) {
        logger(LogLevel::Error) << "Failed to allocate memory for AVFrame"; 
        return -1;
    }

    auto* packet = av_packet_alloc();
    if (!packet) {
        logger(LogLevel::Error) << "Failed to allocate memory for AVPacket";
        return -1;
    }

    int response = 0;
    int packets_to_process = 8;

    while (av_read_frame(format_ctx, packet) >= 0) {
        if (packet->stream_index == stream_index) {
            logger(LogLevel::Debug) << "AVPacket->pts" << packet->pts;
            response = decode_packet(packet, codec_ctx, frame);
            if (response < 0) {
                break;
            }
            if (--packets_to_process <= 0) break;
        }

        av_packet_unref(packet);
    }

    avformat_close_input(&format_ctx);
    avcodec_free_context(&codec_ctx);
    av_frame_free(&frame);
    av_packet_free(&packet);

    return 0;
}

static int decode_packet(AVPacket* packet, AVCodecContext* codec_ctx, AVFrame* frame) {
    int response = avcodec_send_packet(codec_ctx, packet);

    if (response < 0) {
        logger(LogLevel::Error) << "Failed while sending a packet to the decoder";
        return response;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(codec_ctx, frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } else if (response < 0) {
            logger(LogLevel::Error) << "Failed while receiving a frame from the decoder";
            return response;
        }

        if (response >= 0) {
            char frame_filename[1024];
            snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", codec_ctx->frame_number);
            if (frame->format != AV_PIX_FMT_YUV420P) {
                logger(LogLevel::Warning) << "The generated file may not be a grayscale image";
            }

            save_grey_frame(frame->data[0], frame->linesize[0], frame->width, frame->height, frame_filename);
        }
    }
    return 0;
}

static void save_grey_frame(unsigned char* buffer, int wrap, int x_size, int y_size, char* filename) {
    auto* f = fopen(filename, "w");
    fprintf(f, "P5\n%d %d\n%d\n", x_size, y_size, 255);
    for (int i = 0; i < y_size; i++) {
        fwrite(buffer + i*wrap, 1, x_size, f);
    }
    fclose(f);
}
