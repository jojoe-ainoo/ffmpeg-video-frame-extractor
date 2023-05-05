/**
 * @file A3.c
 * @author Emmanuel Ainoo & Elijah Ayomide Oduba
 * @brief 
 * 
 * Getting started with ffmpeg: take one frame from a video file, 
 * transform it into gray scale, then save it into an image file in pgm format.
 * 
 * @version 0.1
 * @date 2022-10-06
 * 
 * @copyright Copyright (c) 2022
 * @cite //https://github.com/leandromoreira/ffmpeg-libav-tutorial#transcoding
 */



#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// #include <cairo.h>
// #include <gtk/gtk.h>

static AVFrame *rgb24_frame = NULL; // use to write raw data source on cairo
static enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P, dst_pix_fmt = AV_PIX_FMT_RGB24;

/**
 * @brief 
 * Function to log messages
 * @param fmt 
 * @param ... 
 */
static void logging(const char *fmt, ...);

/**
 * @brief 
 * Function to decode stream packets into frames
 * @param pPacket 
 * @param pCodecContext 
 * @param pFrame 
 * @return int 
 */
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame);

/**
 * @brief 
 * Function to convert frame into grayscale and save
 * @param buf 
 * @param wrap 
 * @param xsize 
 * @param ysize 
 * @param filename 
 */
static void save_gray_frame(unsigned char *buf, int wrap, int xsize, int ysize, int fnumber);

/**
 * @brief 
 * Function to convert frame into grayscale and save
 * @param buf 
 * @param wrap 
 * @param xsize 
 * @param ysize 
 * @param filename 
 */
// static void save_rgb_frame(unsigned char *buf, uint8_t const * const * data, int lsize, enum AVPixelFormat pix_fmt, int wrap, int xsize, int ysize, char *filename);
static void save_rgb_frame(AVFrame *frame, int fnumber);



static AVFrame* allocateFrame(int width, int height);

int main(int argc, char **argv){

    // Check to make sure filename is passed to the command line
    if (argc < 2) {
        printf("You need to specify a media file.\n");
        return -1; // exit application if no filename is passed
    }

    logging("initializing all the containers, codecs and protocols.");

    // AVFormatContext holds the header information from the format (Container) - Allocating memory for this component
    AVFormatContext *pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        logging("ERROR could not allocate memory for Format Context");
        return -1;
    }

    // Open the file and read its header. The codecs are not opened.
    logging("opening the input file (%s) and loading format (container) header", argv[1]);
    if (avformat_open_input(&pFormatContext, argv[1], NULL, NULL) != 0) {
        logging("ERROR av could not open the file");
        return -1; // exit application if av could not be opened
    }

    // Log some info about file after reading header
    logging("format %s, duration %lld us, bit_rate %lld", pFormatContext->iformat->name, pFormatContext->duration, pFormatContext->bit_rate);
    
    // read Packets from the Format to get stream information, this function populates pFormatContext->streams
    logging("finding stream info from format");
    if (avformat_find_stream_info(pFormatContext,  NULL) < 0) {
        logging("ERROR could not get the stream info");
        return -1;
    }

    AVCodec *pCodec = NULL; // the component that knows how to enCOde and DECode the stream
    AVCodecParameters *pCodecParameters =  NULL;   // this component describes the properties of a codec used by the stream i
    int video_stream_index = -1;

    // loop though all the streams and print its main information
    for (int i = 0; i < pFormatContext->nb_streams; i++)
    {
        AVCodecParameters *pLocalCodecParameters =  NULL;
        pLocalCodecParameters = pFormatContext->streams[i]->codecpar;
        logging("AVStream->time_base before open coded %d/%d", pFormatContext->streams[i]->time_base.num, pFormatContext->streams[i]->time_base.den);
        logging("AVStream->r_frame_rate before open coded %d/%d", pFormatContext->streams[i]->r_frame_rate.num, pFormatContext->streams[i]->r_frame_rate.den);
        logging("AVStream->start_time %" PRId64, pFormatContext->streams[i]->start_time);
        logging("AVStream->duration %" PRId64, pFormatContext->streams[i]->duration);

        logging("finding the proper decoder (CODEC)");

        AVCodec *pLocalCodec = NULL;
        pLocalCodec = avcodec_find_decoder(pLocalCodecParameters->codec_id);   // finds the registered decoder for a codec ID

        if (pLocalCodec==NULL) {
            logging("ERROR unsupported codec!"); // if the codec is not found, just skip it
            continue;
        }

        // when the stream is a video we store its index, codec parameters and codec
        if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (video_stream_index == -1) {
            video_stream_index = i;
            pCodec = pLocalCodec;
            pCodecParameters = pLocalCodecParameters;
        }

        logging("Video Codec: resolution %d x %d", pLocalCodecParameters->width, pLocalCodecParameters->height);
        } else if (pLocalCodecParameters->codec_type == AVMEDIA_TYPE_AUDIO) {
        logging("Audio Codec: %d channels, sample rate %d", pLocalCodecParameters->channels, pLocalCodecParameters->sample_rate);
        }

        // print its name, id and bitrate
        logging("\tCodec %s ID %d bit_rate %lld", pLocalCodec->name, pLocalCodec->id, pLocalCodecParameters->bit_rate);
    } 

    // check file to check if contains video stream 
    if (video_stream_index == -1) {
        logging("File %s does not contain a video stream!", argv[1]);
        return -1;
    }

    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    if (!pCodecContext) {
        logging("failed to allocated memory for AVCodecContext");
        return -1;
    }

    // Fill the codec context based on the values from the supplied codec parameters
    if (avcodec_parameters_to_context(pCodecContext, pCodecParameters) < 0){
        logging("failed to copy codec params to codec context");
        return -1;
    }

    // Initialize the AVCodecContext to use the given AVCodec.
    if (avcodec_open2(pCodecContext, pCodec, NULL) < 0){
        logging("failed to open codec through avcodec_open2");
        return -1;
    }

    AVFrame *pFrame = av_frame_alloc();
    if (!pFrame) {
        logging("failed to allocate memory for AVFrame");
        return -1;
    }

    AVPacket *pPacket = av_packet_alloc();
    if (!pPacket) {
        logging("failed to allocate memory for AVPacket");
        return -1;
    }

    int response = 0;
    int how_many_packets_to_process = 5; // choosing 8 packets to process from the stream

    // fill the Packet with data from the Stream
    while (av_read_frame(pFormatContext, pPacket) >= 0) {
   
        if (pPacket->stream_index == video_stream_index) { // if it's the video stream
            logging("AVPacket->pts %" PRId64, pPacket->pts);
            response = decode_packet(pPacket, pCodecContext, pFrame); // decode packet form stream 
            if (response < 0)
                break; 
    
            if (--how_many_packets_to_process <= 0) break; // stop it when 8 packets are loaded
        }
        av_packet_unref(pPacket); // unreference packet to default values
    }

    logging("releasing all the resources");

    avformat_close_input(&pFormatContext); // close stream input
    av_packet_free(&pPacket); // free packet resources
    av_frame_free(&pFrame); // free frame resources
    avcodec_free_context(&pCodecContext); // free context

    return 0;
}

/**
 * @brief 
 * Function Definition of Logging out Messages
 * @param fmt 
 * @param ... 
 */
static void logging(const char *fmt, ...){
    va_list args;
    fprintf( stderr, "{LOG}:-- " );
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
    fprintf( stderr, "\n" );
}

/**
 * @brief 
 * Function to decode packets from stream 
 * @param pPacket 
 * @param pCodecContext 
 * @param pFrame 
 * @return int 
 */
static int decode_packet(AVPacket *pPacket, AVCodecContext *pCodecContext, AVFrame *pFrame) {
    int response = avcodec_send_packet(pCodecContext, pPacket);   // Supply raw packet data as input to a decoder

    if (response < 0) {
        logging("Error while sending a packet to the decoder: %s", av_err2str(response));
        return response;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(pCodecContext, pFrame);    // Return decoded output data (into a frame) from a decoder
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        } 
        else if (response < 0) {
            logging("Error while receiving a frame from the decoder: %s", av_err2str(response)); // log error message from reponse
            return response;
        }

        if (response >= 0) {
            logging(
                "Frame %d (type=%c, size=%d bytes, format=%d) pts %d key_frame %d [DTS %d]",
                pCodecContext->frame_number,
                av_get_picture_type_char(pFrame->pict_type),
                pFrame->pkt_size,
                pFrame->format,
                pFrame->pts,
                pFrame->key_frame,
                pFrame->coded_picture_number
            );

      
        // Check if the frame is a planar YUV 4:2:0, 12bpp // That is the format of the provided .mp4 file
        // RGB formats will definitely not give a gray image // Other YUV image may do so, but untested, so give a warning
        if (pFrame->format != AV_PIX_FMT_YUV420P) 
            logging("Warning: the generated file may not be a grayscale image, but could e.g. be just the R component if the video format is RGB");
        
        //convert frame to RGG format
        // pFrame = avFrameConvertPixelFormat(pFrame, AV_PIX_FMT_RGB24);

        // save a grayscale frame into a .pgm file
        save_gray_frame(pFrame->data[0], pFrame->linesize[0], pFrame->width, pFrame->height, pCodecContext->frame_number);
        //save_rgb_frame(pFrame->data[0], pFrame->data, Frame->linesize, (enum AVPixelFormat)pFrame->format, pFrame->linesize[0], pFrame->width, pFrame->height, frame_filename);
        save_rgb_frame(pFrame, pCodecContext->frame_number);
        //refreshWindow
        }
    }
    return 0; //exit 
}

//convert to rgba (contextWidth, contextHeight,)

/**
 * @brief 
 * Function to save the frame as a grayscale
 * @param buf 
 * @param wrap 
 * @param xsize 
 * @param ysize 
 * @param filename 
 */
static void save_gray_frame(unsigned char *buf,  int wrap, int xsize, int ysize, int fnumber) {
    char frame_filename[1024];
    snprintf(frame_filename, sizeof(frame_filename), "%s-%d.pgm", "frame", fnumber);
    char *filename = frame_filename;
    FILE *f;
    int i;
    f = fopen(filename,"w"); // writing the minimal required header for a pgm file format
    
      // portable graymap format -> https://en.wikipedia.org/wiki/Netpbm_format#PGM_example
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

    // writing line by line
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}


static void save_rgb_frame(AVFrame *pFrame, int fnumber) {
    char frame_filename[1024];
    snprintf(frame_filename, sizeof(frame_filename), "%s-%d.ppm", "frame", fnumber);
    char *filename = frame_filename;
    FILE *f;
    int i;
    f = fopen(filename,"w"); // writing the minimal required header for a pgm file format
    
    // write header
    fprintf(f, "P6\n%d %d\n255\n", pFrame->width, pFrame->height);

    // create scaling to convert to rgb
    AVFrame* frame_rgb = allocateFrame(pFrame->width, pFrame->height);
    // source(src)=> pFrame  & destination(dst) => frame_rgb
    // refer: https://ffmpeg.org/doxygen/3.1/scaling_video_8c-example.html
    // use swscale for conversion  ->  sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt, dst_w, dst_h, dst_pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    struct SwsContext* converted_data = sws_getContext(pFrame->width, pFrame->height, pFrame->format, frame_rgb->width,frame_rgb->height, dst_pix_fmt, SWS_FAST_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND, NULL, NULL, NULL);
    sws_scale(converted_data,(uint8_t const * const *)pFrame->data, pFrame->linesize, 0, pFrame->height, frame_rgb->data, frame_rgb->linesize);
    sws_freeContext(converted_data);

    for (i = 0; i < frame_rgb->height; i++) 
        fwrite(frame_rgb->data[0] + i * frame_rgb->linesize[0], 1, frame_rgb->width * 3, f);
    fclose(f);
}

static AVFrame* allocateFrame(int width, int height){

    AVFrame* newFrame = av_frame_alloc();
    if (newFrame == NULL)
        fprintf(stderr, "could not allocate destination frame");
  
    if (av_image_alloc(newFrame->data, newFrame->linesize, width, height, dst_pix_fmt, 1) < 0) 
        fprintf(stderr, "Could not allocate destination image");
  
    newFrame->width = width;
    newFrame->height = height;
    newFrame->format = dst_pix_fmt;

    return newFrame;
}





//75% DONE

// producer():
// define buffer[]
//define max_frames=num_of_packets_to_process, lock, size
// processPacket() => buffer => PRODUCER_LOGIC
// for(i < maxItems)
//      decodeFrame()
//      wait()
//      lock()
//      addFrameToBuffer()
//      increase bufferSize
//      unlock()
//      signal()

// consumer():
// timer()
// 

