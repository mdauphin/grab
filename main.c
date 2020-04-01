#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
//#include <libswresample/swresample.h>
#include <libswscale/swscale.h>

AVFormatContext *m_pFormatCtx;
AVCodecContext  *m_pCodecCtx;
AVFrame         *m_pFrame; 
AVPicture		m_picture ;	
AVPicture		m_pictureRGB ;	
struct SwsContext 	*m_swsContext;

int m_videoStream ;

int openVideo(char* url)
{
    AVCodec *pCodec = NULL ;
    
	if ( avformat_open_input( &m_pFormatCtx, url, NULL, NULL ) != 0 )
	{
		printf("Error opening file %s\n", url );
		return 1 ;
	}

	if(avformat_find_stream_info(m_pFormatCtx,NULL)<0)
	{
		printf("Error find stream info\n");
		return 1;
	}

	m_videoStream = av_find_best_stream( m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);
	if ( m_videoStream == -1 )
	{
		printf("Error did not find video stream\n");
		return 1;
	}
	m_pCodecCtx = m_pFormatCtx->streams[m_videoStream]->codec;
	if ( avcodec_open2(m_pCodecCtx, pCodec, NULL) < 0) {
		printf("Error open codec\n");
		return 0;
	}
	m_pFrame = av_frame_alloc();
	if ( m_pFrame == NULL ) {
		printf("Error alloc frame\n");
		return 1;
	}
    
	if ( avpicture_alloc( &m_picture, m_pCodecCtx->pix_fmt, m_pCodecCtx->width, m_pCodecCtx->height ) < 0 )
	{
		printf("Error alloc avpicture\n");
		return 1;
	}
    
    if ( avpicture_alloc( &m_pictureRGB, AV_PIX_FMT_YUVJ420P, m_pCodecCtx->width, m_pCodecCtx->height ) < 0 )
    {
		printf("Error alloc avpicture\n");
		return 1;        
    }
    
	//Allocate swscale Context for Codec Format to RGB32 convertion
	m_swsContext = sws_getContext( m_pCodecCtx->width, m_pCodecCtx->height, m_pCodecCtx->pix_fmt,
		m_pCodecCtx->width, m_pCodecCtx->height, PIX_FMT_YUVJ420P, SWS_FAST_BILINEAR, NULL, NULL, NULL);    
        //AV_PIX_FMT_YUVJ420P
	
	
}

int closeVideo() {
	if ( m_pFrame )
		av_free( m_pFrame );
	m_pFrame = NULL ;
	// Close the codec
	if ( m_pCodecCtx ) 
		avcodec_close(m_pCodecCtx);
	m_pCodecCtx = NULL ;
	// Close the video file
	if ( m_pFormatCtx )
		avformat_close_input(&m_pFormatCtx);
	m_pFormatCtx = NULL ;
	return 0 ;
}

int convert(AVFrame *frame, int width, int height, int pxl_fmt) 
{
    AVPacket pkt;
    AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!codec)
    {
        printf("convert not find encoder\n");
        return 1 ;
    }
    AVCodecContext* cxt = avcodec_alloc_context3(codec);
    if (!cxt)
    {
        printf("convert can not alloc context\n");
        return 1 ;
    }
    
    cxt->bit_rate = 400000;
    cxt->width = width;
    cxt->height = height;
    cxt->time_base= (AVRational){1,25};
    cxt->pix_fmt = pxl_fmt ; //AV_PIX_FMT_YUVJ420P;    
    
    if ( avcodec_open2(cxt, codec, NULL)<0 )
    {
        printf("Could not open codec\n");
        return 1 ;        
    }
    
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    frame->pts = 1 ;
    
    int got_output = 0;
    avcodec_encode_video2(cxt, &pkt, frame, &got_output);
    
    if (got_output)
    {
        printf("got frame\n");
        FILE* f = fopen("output.jpeg", "wb");
        fwrite(pkt.data, 1, pkt.size, f);
        av_free_packet(&pkt);
    }

    avcodec_close(cxt);
    av_free(cxt);
    av_freep(&frame->data[0]);
    return 0;
}

int getNextFrame()
{
	AVFormatContext *pFormatCtx = m_pFormatCtx ;
	AVCodecContext *pCodecCtx = m_pCodecCtx ;
	AVFrame *pFrame = m_pFrame ;
	AVPacket packet;
	int bytesDecoded=0;
	int ret = 0 ;
	while(av_read_frame(pFormatCtx, &packet)>=0) 
	{
		if(packet.stream_index==m_videoStream) 
		{
			int frameFinished = 0 ;
			bytesDecoded = avcodec_decode_video2( pCodecCtx, pFrame, &frameFinished, &packet );
			if ( bytesDecoded < 0 ) {
				printf("Error: decode_video2");
				break;
			}
			if (frameFinished) {
                printf("frameFinished %dx%d\n", m_pFrame->width, m_pFrame->height);
                
                av_picture_copy( &m_picture, (AVPicture*)m_pFrame, m_pCodecCtx->pix_fmt, m_pFrame->width, m_pFrame->height );
                
                sws_scale( m_swsContext, (const uint8_t* const*)m_picture.data, m_picture.linesize,
                    0,m_pCodecCtx->height, m_pictureRGB.data, m_pictureRGB.linesize );	
                    
                convert( (AVFrame*)&m_pictureRGB, pFrame->width, pFrame->height, AV_PIX_FMT_YUVJ420P );
				
				break ;
			}
		}
		av_free_packet(&packet);
	}
	av_free_packet(&packet);
	return ret ;
}




int main(int argc, char** argv)
{
	avcodec_register_all();
	av_register_all();

	avformat_network_init();
    
    openVideo(argv[1]);
    
    getNextFrame();
    
    closeVideo();
    
	//avpicture_free(&m_pictureRGB) ;
	avpicture_free(&m_picture) ;
	if ( m_pFrame )
		av_free( m_pFrame );
	m_pFrame = NULL ;
	// Close the codec
	if ( m_pCodecCtx )
		avcodec_close(m_pCodecCtx);
	m_pCodecCtx = NULL ;
	// Close the video file
	if ( m_pFormatCtx )
		avformat_close_input(&m_pFormatCtx); 
	m_pFormatCtx = NULL ;    

	return 0 ;
}
