#define WIN32_LEAN_AND_MEAN
#define SOGL_MAJOR_VERSION 4
#define SOGL_MINOR_VERSION 5
#define SOGL_IMPLEMENTATION_WIN32
#include "../opengl/simple-opengl-loader.h"
#include <windows.h>
#include <GL/gl.h>
#include "wglext.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>


typedef struct {
    float ratio;
    int   texture_width;
    int   texture_height;
} FrameData;

// NOTE: Order is important
#include "../opengl/opengl_render.c"

bool running = true;
uint64_t g_perf_counter_frequency;

#define DECLARE_WGL_EXT_FUNC( returnType, name, ... ) typedef returnType ( WINAPI *name##FUNC )( __VA_ARGS__ );\
    name##FUNC name = ( name##FUNC )0;
#define LOAD_WGL_EXT_FUNC( name ) name = ( name##FUNC ) wglGetProcAddress( #name )

DECLARE_WGL_EXT_FUNC( BOOL, wglChoosePixelFormatARB, HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats );
DECLARE_WGL_EXT_FUNC( HGLRC, wglCreateContextAttribsARB, HDC hDC, HGLRC hshareContext, const int *attribList );

const WCHAR WIN_CLASS_NAME[] = L"OPENGL_WINDOW_CLASS";

LRESULT CALLBACK winProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam ) {
    switch( message ) {
        /* NOTE: We don't need this as it does not handle when clicking maximize button
        case WM_SIZING: {
            if( glViewport ) {
                RECT * bounds = ( RECT * )lParam;
                UINT width = bounds->right - bounds->left;
                UINT height = bounds->bottom - bounds->top;
                glViewport( 0, 0, width, height );
            }
            return 0;
        } break;
        */

        case WM_WINDOWPOSCHANGING: {
            WINDOWPOS * window_position = ( WINDOWPOS * )lParam;
            UINT width = window_position->cx;
            UINT height = window_position->cy;
            if( glViewport && width != 0 && height != 0 ) {
                glViewport( 0, 0, width, height );
            }
            return 0;
        } break;

        case WM_PAINT: {
            if( glClear ) {
                HDC device_context = GetDC( window );
                glClear( GL_COLOR_BUFFER_BIT );
                SwapBuffers( device_context );
            }
        } break;

        case WM_CLOSE: {
            PostQuitMessage( 0 );
            // NOTE: Since we can stuck in decoding frame loop we use
            // this variable for closing the window
            running = false;
            return 0;
        } break;
    }


    return DefWindowProc(window, message, wParam, lParam);
}

uint64_t
get_performance_counter() {
    LARGE_INTEGER result;
    QueryPerformanceCounter( &result );
    return result.QuadPart;
}

double
get_seconds_elapsed( uint64_t Start, uint64_t End ) {
    double result = 0;
    result = ( double )( End - Start ) / ( double )g_perf_counter_frequency;
    return result;
}

int WINAPI
wWinMain( HINSTANCE instance, HINSTANCE prevInstance,
          LPWSTR cmdLine, int showWindow ) {
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

    // NOTE: Attaching to the current console so the ffmpeg can log to the console
    // and printing new line for fancy output :)
    AttachConsole( ATTACH_PARENT_PROCESS );
    av_log_set_flags( AV_LOG_SKIP_REPEATED );

    if( *cmdLine == 0 ) {
        av_log( NULL, AV_LOG_INFO, "\r\nUsage: ffmpeg_player.exe full_path_to_file_name.whatever_extension\r\n" );
        return 0;
    }

    LARGE_INTEGER counter_per_second;
    QueryPerformanceFrequency( &counter_per_second );
    g_perf_counter_frequency = counter_per_second.QuadPart;

    avformat_network_init();
    av_format_ctx = avformat_alloc_context();

    // NOTE: Converting our wchar to char to pass it to ffmpeg API
    int length = WideCharToMultiByte( CP_ACP, 0, cmdLine,
                                      -1, NULL, 0, NULL, NULL );
    char file_path[MAX_PATH] = {0};
    length = WideCharToMultiByte( CP_ACP, 0, cmdLine,
                                  -1, file_path, length, NULL, NULL );
    file_path[length] = '\0';

    // NOTE: This is a hack to delete quotes from command line because
    // avformat_open_input cannot handle that
    if( file_path[0] == '\'' || file_path[0] == '"' ) {
        int count = 0;
        while( count < length ) {
            file_path[count] = file_path[count + 1];
            ++count;
        }
        file_path[count - 3] = '\0';
    }

    if( avformat_open_input( &av_format_ctx, file_path, NULL, NULL ) != 0 ) {
        // TODO: Error
        return -1;
    }

    if( avformat_find_stream_info( av_format_ctx, NULL ) < 0 ) {
        // TODO: Error
        return -1;
    }

    video_index = -1;
    for( int i = 0; i < av_format_ctx->nb_streams; ++i ) {
        if( av_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ) {
            video_index = i;
            break;
        }
    }

    if( video_index == -1 ){
        // TODO: Error
        return -1;
    }

    av_codec = avcodec_find_decoder( av_format_ctx->streams[video_index]->codecpar->codec_id );
    if( av_codec == NULL ) {
        // TODO: Error
        return -1;
    }

    AVStream * video_stream = av_format_ctx->streams[video_index];
    double timebase = video_stream->time_base.num * 1000.0 / video_stream->time_base.den;

    av_codec_ctx = avcodec_alloc_context3( av_codec );
    int res = avcodec_parameters_to_context( av_codec_ctx, video_stream->codecpar );
    if( res < 0 ) {
        // TODO: Error
        return -1;
    }

    if( avcodec_open2( av_codec_ctx, av_codec, NULL ) < 0 ) {
        // TODO: Error
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

    img_convert_ctx = sws_getContext( av_codec_ctx->width,
                                      av_codec_ctx->height,
                                      av_codec_ctx->pix_fmt,
                                      av_codec_ctx->width,
                                      av_codec_ctx->height,
                                      AV_PIX_FMT_YUV420P,
                                      SWS_BICUBIC, NULL, NULL, NULL );

    if( !img_convert_ctx ) {
        // TODO: Error
        return 1;
    }

    packet=( AVPacket * )av_malloc( sizeof( AVPacket ) );

    WNDCLASSEX window_class = {0};
    window_class.cbSize = sizeof( window_class );
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = winProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIcon( instance, IDI_APPLICATION );
    window_class.hIconSm = LoadIcon( instance, IDI_APPLICATION );
    window_class.hCursor = LoadCursor( NULL, IDC_ARROW );
    window_class.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    window_class.lpszClassName = WIN_CLASS_NAME;

    if ( !RegisterClassEx( &window_class ) ) {
        MessageBox( NULL, L"Failed to register window class!", L"FAILURE", MB_OK );
        return 1;
    }

    // NOTE:  Create a dummy window so we can get WGL extension functions
    HWND dummy_window = CreateWindow( WIN_CLASS_NAME, L"DUMMY",
                                      WS_OVERLAPPEDWINDOW, 0, 0,
                                      1, 1, NULL,  NULL, instance, NULL );

    if ( !dummy_window ) {
        MessageBox( NULL, L"Failed to create window!", L"FAILURE", MB_OK );
        return 1;
    }

    HDC dummy_context = GetDC( dummy_window );

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof( PIXELFORMATDESCRIPTOR );
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat( dummy_context, &pfd );
    SetPixelFormat( dummy_context, pixelFormat, &pfd );
    HGLRC dummy_gl = wglCreateContext( dummy_context );
    wglMakeCurrent( dummy_context, dummy_gl );

    LOAD_WGL_EXT_FUNC( wglChoosePixelFormatARB );
    LOAD_WGL_EXT_FUNC( wglCreateContextAttribsARB );

    if ( !wglCreateContextAttribsARB || !wglCreateContextAttribsARB ) {
        MessageBox( NULL, L"Didn't get wgl ARB functions!", L"FAILURE", MB_OK );
        return 1;
    }

    wglMakeCurrent( NULL, NULL );
    wglDeleteContext( dummy_gl );
    DestroyWindow( dummy_window );

    // NOTE: Create real window and rendering context
    HWND window = CreateWindow(
            WIN_CLASS_NAME,
            L"Simple ffmpeg player",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            av_codec_ctx->width, av_codec_ctx->height,
            NULL,
            NULL,
            instance,
            NULL
            );

    if ( !window ) {
        MessageBox( NULL, L"Failed to create window!", L"FAILURE", MB_OK );
        return 1;
    }

    HDC device_context = GetDC( window );

    const int pixelAttribList[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
        WGL_SAMPLES_ARB, 4,
        0
    };

    UINT num_formats;
    BOOL success;
    success = wglChoosePixelFormatARB( device_context, pixelAttribList, NULL,
                                       1, &pixelFormat, &num_formats );

    if ( !success || num_formats == 0 ) {
        MessageBox( NULL, L"Didn't get ARB pixel format!", L"FAILURE", MB_OK );
        return 1;
    }

    DescribePixelFormat( device_context, pixelFormat, sizeof( pfd ), &pfd );
    SetPixelFormat( device_context, pixelFormat, &pfd );

    const int contextAttribList[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 5,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    HGLRC gl = wglCreateContextAttribsARB( device_context, NULL, contextAttribList );

    if ( !gl ) {
        MessageBox( NULL, L"Didn't get ARB GL context!", L"FAILURE", MB_OK );
        return 1;
    }

    wglMakeCurrent( device_context, gl );

    if ( !sogl_loadOpenGL() ) {
        const char * * failures = sogl_getFailures();
        while ( *failures++ ) {
            char debugMessage[256];
            snprintf( debugMessage, 256,
                      "SOGL WIN32 EXAMPLE: Failed to load function %s\n",
                      *failures );
            OutputDebugStringA( debugMessage );
        }
    }

    glClearColor( 0.0, 0.0, 0.0, 1.0 );

    unsigned int textures[3];
    opengl_generate_texture( textures );
    opengl_make_program();

    ShowWindow( window, showWindow );

    uint64_t start_time = get_performance_counter();

    // NOTE: Start render and message loop
    MSG message;
    while( running ) {
        while( av_read_frame( av_format_ctx, packet ) >= 0 ) {

            if( packet->stream_index != video_index ) {
                av_packet_unref( packet );
                continue;
            }

            opengl_render();

            bool okay = false;
            while( !okay ) {
                int ret = avcodec_send_packet( av_codec_ctx, packet );
                if ( ret >= 0 ) {
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
                               frame_copy->data, frame_copy->linesize);
                    copy_frame_to_texture( frame_copy, textures );

                    while( PeekMessageA( &message, 0, 0, 0, PM_REMOVE ) ) {
                        TranslateMessage( &message );
                        DispatchMessage( &message );
                    }

                    if( !running ) goto END;

                    // NOTE: We have to calculate time before we unref our frame
                    uint64_t elapsed_microseconds = get_seconds_elapsed( start_time, get_performance_counter() ) * 1000000;
                    uint64_t frame_pts = 1000000 * timebase * frame->pts / 1000;

                    av_frame_unref( frame );
                    av_frame_unref( frame_copy );
                    // NOTE: It assumes our CPU fast enough to decode, so frame
                    // presentation timestamp should be always greater
                    // than real elapsed time
                    if( frame_pts > elapsed_microseconds )
                        Sleep( ( frame_pts - elapsed_microseconds )/1000 );
                }
            }

            SwapBuffers( device_context );

            av_packet_unref( packet );
        }

        // NOTE: All frames are decoded
        running = false;
    }

END:
    FreeConsole();
    sws_freeContext( img_convert_ctx );
    avcodec_free_context( &av_codec_ctx );
    av_frame_free( &frame_copy );
    av_frame_free( &frame );
    avcodec_close( av_codec_ctx );
    avformat_close_input( &av_format_ctx );

    return (int) message.wParam;
}
