#define SOGL_MAJOR_VERSION 4
#define SOGL_MINOR_VERSION 5
#define SOGL_IMPLEMENTATION_X11
#include "../opengl/simple-opengl-loader.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <X11/Xlib.h>
#include <GL/glx.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include <unistd.h>
#include <time.h> // time precision Linux


typedef struct {
    float ratio;
    int texture_width;
    int texture_height;
} FrameData;

// NOTE: Order is important
#include "../opengl/opengl_render.c"

typedef GLXContext ( * glXCreateContextAttribsARBFUNC )( Display*,
                                                         GLXFBConfig,
                                                         GLXContext,
                                                         Bool,
                                                         const int*);

int
main( int argc, char const * argv[] ) {
    if( argc <= 1 ) {
        fprintf( stdout, "Usage: ./ffmpeg_player full_path_to_file_name.whatever_extension\n" );
        return 0;
    }

    AVFormatContext   * av_format_ctx;
    AVCodecContext    * av_codec_ctx;
    const AVCodec     * av_codec;
    AVFrame           * frame;
    AVFrame           * frame_copy;
    AVPacket          * packet;
    struct SwsContext * img_convert_ctx;
    unsigned char     * buffer;
    int                 video_index;
    FrameData           frame_data;

    Display           * display;
    Window              window;
    XEvent              event;
    XWindowAttributes   x_window_attributes;

    frame_data.ratio = 1.0f;
    frame_data.texture_width = -1;
    frame_data.texture_height = -1;

    /* FFmpeg stuff */
    av_log_set_flags( AV_LOG_SKIP_REPEATED );
    //av_log_set_level( AV_LOG_QUIET );
    avformat_network_init();
    av_format_ctx = avformat_alloc_context();

    if( avformat_open_input( &av_format_ctx, argv[1] ,NULL, NULL ) != 0 ) {
        fprintf( stderr, "Couldn't open input stream.\n" );
        return -1;
    }

    if( avformat_find_stream_info( av_format_ctx, NULL ) < 0 ) {
        fprintf( stderr, "Couldn't find stream information.\n" );
        return -1;
    }

    video_index = -1;
    for( int i = 0; i < av_format_ctx->nb_streams; ++i ) {
        if( av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ) {
            video_index=i;
            break;
        }
    }

    if( video_index == -1 ){
        fprintf( stderr, "Didn't find a video stream.\n" );
        return -1;
    }

    av_codec = avcodec_find_decoder( av_format_ctx->streams[video_index]->codecpar->codec_id );
    if( av_codec == NULL ) {
        fprintf( stderr, "Codec not found.\n" );
        return -1;
    }

    AVStream * video_stream = av_format_ctx->streams[video_index];
    double timebase = video_stream->time_base.num * 1000.0 / video_stream->time_base.den;

    av_codec_ctx = avcodec_alloc_context3( av_codec );
    int res = avcodec_parameters_to_context( av_codec_ctx, video_stream->codecpar );
                                            //av_format_ctx->streams[video_index]->codecpar );

    if( res < 0 ) {
        fprintf( stderr, "Error while filling codec context from codec parameters\n" );
        return -1;
    }

    if( avcodec_open2( av_codec_ctx, av_codec, NULL ) < 0 ) {
        fprintf( stderr, "Could not open codec.\n" );
        return -1;
    }

    frame = av_frame_alloc();
    frame_copy = av_frame_alloc();

    buffer = ( unsigned char * )av_malloc( av_image_get_buffer_size( AV_PIX_FMT_YUV420P,
                                                                     av_codec_ctx->width,
                                                                     av_codec_ctx->height,
                                                                     1 ) );
    av_image_fill_arrays( frame_copy->data, frame_copy->linesize, buffer,
                          AV_PIX_FMT_YUV420P, av_codec_ctx->width,
                          av_codec_ctx->height, 1 );

    /* If you need print file information
    printf("---------------- File Information ---------------\n");
    av_dump_format(av_format_ctx,0,argv[1],0);
    printf("-------------------------------------------------\n");
    */

    img_convert_ctx = sws_getContext( av_codec_ctx->width,
                                      av_codec_ctx->height,
                                      av_codec_ctx->pix_fmt,
                                      av_codec_ctx->width,
                                      av_codec_ctx->height,
                                      AV_PIX_FMT_YUV420P,
                                      SWS_BICUBIC, NULL, NULL, NULL);

    if( !img_convert_ctx ) {
        fprintf( stderr, "Cannot create image context with sws_getContext\n" );
        return 1;
    }

    packet=( AVPacket * )av_malloc( sizeof( AVPacket ) );

    /* X Windows stuff */
    display = XOpenDisplay( NULL );

    if ( display == NULL ) {
        fprintf( stderr, "Unable to connect to X Server\n" );
        return 1;
    }

    window = XCreateSimpleWindow( display,
                                  DefaultRootWindow( display ),
                                  20,
                                  20,
                                  av_codec_ctx->width,
                                  av_codec_ctx->height,
                                  0, 0, 0);


    XSelectInput( display, window, ExposureMask | KeyPressMask | ButtonPressMask );
    XStoreName( display, window, "Simple ffmpeg player" );
    XMapWindow( display, window );

    /* OpenGL stuff */
    //opengl_init( display, window );
    int num_fbc = 0;
    GLint visual_attributes[] = {
        GLX_RENDER_TYPE,   GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,  True,
        GLX_RED_SIZE,      1,
        GLX_GREEN_SIZE,    1,
        GLX_BLUE_SIZE,     1,
        GLX_DEPTH_SIZE,    1,
        GLX_STENCIL_SIZE,  1,
        None
    };

    GLXFBConfig * fbc = glXChooseFBConfig( display, DefaultScreen(display),
                                           visual_attributes, &num_fbc);

    if ( !fbc ) {
        fprintf( stderr, "Unable to get framebuffer\n" );
        return 1;
    }

    glXCreateContextAttribsARBFUNC glXCreateContextAttribsARB = ( glXCreateContextAttribsARBFUNC ) glXGetProcAddress( ( const GLubyte * ) "glXCreateContextAttribsARB" );

    if ( !glXCreateContextAttribsARB ) {
        fprintf( stderr, "Unable to get proc glXCreateContextAttribsARB\n" );
        XFree( fbc );
        return 1;
    }

    static int context_attributes[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 5,
        GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXContext ctx = glXCreateContextAttribsARB( display, * fbc, NULL,
                                                 True, context_attributes );

    XFree( fbc );

    if ( !ctx ) {
        fprintf( stderr, "Unable to create OpenGL context\n" );
        return 1;
    }

    glXMakeCurrent( display, window, ctx );

    if ( !sogl_loadOpenGL() ) {
        const char * * failures = sogl_getFailures();
        while ( *failures++ ) fprintf( stderr, "Failed to load function %s\n",
                                       *failures );
    }

    glClearColor( 0.0, 0.0, 0.0, 1.0 );

    unsigned int textures[3];
    opengl_generate_texture( textures );
    opengl_make_program();

    Atom wmDeleteMessage = XInternAtom( display, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( display, window, &wmDeleteMessage, 1 );

    // NOTE: tv_sec, tv_nsec
    struct timespec start_time = {0};
    clock_gettime( CLOCK_MONOTONIC, &start_time );
    uint64_t start_time_nanoseconds = ( start_time.tv_sec * 1000000000 ) + start_time.tv_nsec;

    // Animation loop
    while ( av_read_frame( av_format_ctx, packet ) >= 0 ) {
        if( packet->stream_index != video_index ) {
            av_packet_unref( packet );
            continue;
        }

        opengl_render();

        if ( XCheckTypedWindowEvent( display, window, Expose, &event ) == True ) {
            XGetWindowAttributes( display, window, &x_window_attributes );
            glViewport( 0, 0, x_window_attributes.width, x_window_attributes.height );
        }

        if ( XCheckTypedWindowEvent( display, window, ClientMessage, &event ) == True ) {
            if ( event.xclient.data.l[0] == wmDeleteMessage ) {
                break;
            }
        }

        bool okay = false;
        while( !okay ) {
            int ret = avcodec_send_packet( av_codec_ctx, packet );
            if( ret >= 0 ) {
                okay = true;
                av_packet_unref( packet );
            }

            while( ( ret = avcodec_receive_frame( av_codec_ctx, frame ) ) >= 0 ) {
                frame_copy->width = frame->width;
                frame_copy->height = frame->height;
                frame_copy->format = AV_PIX_FMT_YUV420P;
                // NOTE: According to ffmpeg documentation we can set our
                // private data, so we use it to get later our texture
                // width/height and ratio and avoiding global variables
                frame_copy->opaque = &frame_data;
                av_frame_get_buffer( frame_copy, 0 );
                sws_scale( img_convert_ctx,
                          ( const unsigned char * const * )frame->data,
                          frame->linesize, 0, av_codec_ctx->height,
                          frame_copy->data, frame_copy->linesize );
                copy_frame_to_texture( frame_copy, textures );

                // NOTE: We have to calculate time before we unref our frame
                struct timespec elapsed = {0};
                clock_gettime( CLOCK_MONOTONIC, &elapsed );
                uint64_t elapsed_nanoseconds = elapsed.tv_nsec + ( elapsed.tv_sec * 1000000000 ) - start_time_nanoseconds;
                double eps = elapsed_nanoseconds / 1000000000.0;
                double frame_pts = timebase * frame->pts / 1000.0;

                av_frame_unref( frame );
                av_frame_unref( frame_copy );
                // NOTE: microseconds
                // It assumes our CPU fast enough to decode, so frame
                // presentation timestamp should be always greater
                // than real elapsed time
                if( frame_pts > eps )
                    usleep( ( ( frame_pts - eps ) * 1000000 ) );
            }
        }

        glXSwapBuffers(display, window);

        av_packet_unref( packet );
    }

    // Teardown
    sws_freeContext( img_convert_ctx );
    avcodec_free_context( &av_codec_ctx );
    av_frame_free( &frame_copy );
    av_frame_free( &frame );
    avcodec_close( av_codec_ctx );
    avformat_close_input( &av_format_ctx );

    XDestroyWindow( display, window );
    XCloseDisplay( display );
}
