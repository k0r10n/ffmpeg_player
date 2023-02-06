const char * vs_source =
"#version 330 core\n"
"layout ( location = 0 ) in vec4 aPos;\n"
"layout ( location = 1 ) in vec2 aTexCoord;\n"
"out vec2 TexCoord;\n"
"void main() {\n"
"    gl_Position = aPos;\n"
"    TexCoord = vec2( aTexCoord.x, aTexCoord.y );\n"
"}\n";

const char * fs_source =
"#version 330 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D textureY;\n"
"uniform sampler2D textureU;\n"
"uniform sampler2D textureV;\n"
"void main() {\n"
"    vec3 yuv, rgb;\n"
"    vec3 yuv2r = vec3( 1.164, 0.0, 1.596 );\n"
"    vec3 yuv2g = vec3( 1.164, -0.391, -0.813 );\n"
"    vec3 yuv2b = vec3( 1.164, 2.018, 0.0 );\n"
"    yuv.x = texture( textureY, TexCoord).r - 0.0625;\n"
"    yuv.y = texture( textureU, TexCoord).r - 0.5;\n"
"    yuv.z = texture( textureV, TexCoord).r - 0.5;\n"
"    rgb.x = dot( yuv, yuv2r );\n"
"    rgb.y = dot( yuv, yuv2g );\n"
"    rgb.z = dot( yuv, yuv2b );\n"
"    FragColor = vec4( rgb, 1.0 );\n"
"}\n";

void
opengl_generate_texture( unsigned int * textures ) {
    glGenTextures( 3, textures );
    for( int i = 0; i < 3; ++i ) {
        glBindTexture( GL_TEXTURE_2D, textures[i] );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    }
}

GLuint
opengl_create_compile_vertext_shader( const char * vs_source ) {
    GLuint result = glCreateShader( GL_VERTEX_SHADER );
    glShaderSource( result, 1, &vs_source, NULL );
    glCompileShader( result );
    int success = 0;
    glGetShaderiv( result, GL_COMPILE_STATUS, &success );
    if( !success ){
        GLchar info_log[1024];
        glGetShaderInfoLog( result, 1024, NULL, info_log );
        fprintf( stderr, "Failed to compile vertex shader: %s\n", info_log );
    }
    return result;
}

GLuint
opengl_create_compile_fragment_shader( const char * fs_source ) {
    GLuint result = glCreateShader( GL_FRAGMENT_SHADER );
    glShaderSource( result, 1, &fs_source, NULL );
    glCompileShader( result );
    int success = 0;
    glGetShaderiv( result, GL_COMPILE_STATUS, &success );
    if( !success ){
        GLchar info_log[1024];
        glGetShaderInfoLog( result, 1024, NULL, info_log );
        fprintf( stderr, "Failed to compile vertex shader: %s\n", info_log );
    }
    return result;
}

void
opengl_make_program( void ) {
    GLuint vertex_shader = opengl_create_compile_vertext_shader( vs_source );
    GLuint fragment_shader = opengl_create_compile_fragment_shader( fs_source );

    GLuint program = glCreateProgram();
    glAttachShader( program, vertex_shader );
    glAttachShader( program, fragment_shader );
    glLinkProgram( program );

    int params = -1;
    glGetProgramiv( program, GL_LINK_STATUS, &params );

    if ( params != GL_TRUE ) {
        fprintf( stderr, "Program did not link!\n" );
    }

    glDeleteShader( vertex_shader );
    glDeleteShader( fragment_shader );

    glUseProgram( program );

    glUniform1i( glGetUniformLocation( program, "textureY" ), 0 );
    glUniform1i( glGetUniformLocation( program, "textureU" ), 1 );
    glUniform1i( glGetUniformLocation( program, "textureV" ), 2 );
}

void
opengl_render( void ) {
    GLuint dummy_vertex_array_object = 0;
    glGenVertexArrays( 1, &dummy_vertex_array_object );
    glBindVertexArray( dummy_vertex_array_object );

    GLfloat vertices[] = {
        1.0,  1.0, 0.0,    1.0, 0.0,
        1.0, -1.0, 0.0,    1.0, 1.0,
        -1.0, -1.0, 0.0,    0.0, 1.0,
        -1.0,  1.0, 0.0,    0.0, 0.0,
    };

    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    GLuint dummy_vertex_buffer_object = 0;
    GLuint element_array_buffer_object = 0;

    glGenBuffers( 1, &dummy_vertex_buffer_object );
    glGenBuffers( 1, &element_array_buffer_object );

    glBindBuffer( GL_ARRAY_BUFFER, dummy_vertex_buffer_object );
    glBufferData( GL_ARRAY_BUFFER, 4 * 5 * sizeof( GLfloat ),
                 vertices, GL_DYNAMIC_DRAW );

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_object );
    glBufferData( GL_ELEMENT_ARRAY_BUFFER, 2 * 3 * sizeof( unsigned int ),
                 indices, GL_STATIC_DRAW );

    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof( float ), NULL );
    glEnableVertexAttribArray( 0 );

    glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof( float ),
                          ( void * )( 3 * sizeof( float ) ) );
    glEnableVertexAttribArray( 1 );
}

void
copy_frame_to_texture( AVFrame * Frame, unsigned int * textures ) {
    FrameData * frame_data = ( FrameData * )Frame->opaque;

    bool changed = false;
    if( frame_data->texture_width != Frame->width ) {
        frame_data->texture_width = Frame->width;
        changed = true;
    }

    if( frame_data->texture_height != Frame->height ) {
        frame_data->texture_height = Frame->height;
        changed = true;
    }

    float tr = ( float )frame_data->texture_width / frame_data->texture_height;
    float vr = ( float )Frame->width / Frame->height;
    float r = tr / vr;
    float vertices[] = {
        1.0, 0.0, 0.0, 1.0, 0.0,
        1.0, 0.0, 0.0, 1.0, 1.0,
        -1.0, 0.0, 0.0, 0.0, 1.0,
        -1.0, 0.0, 0.0, 0.0, 0.0,
    };

    if( r != frame_data->ratio ) {
        frame_data->ratio = r;
        if( tr > vr ) {
            float p = vr / tr;
            vertices[1] = p;
            vertices[6] = -p;
            vertices[11] = -p;
            vertices[16] = p;
        } else if ( tr < vr ) {
            vertices[0] = r;
            vertices[1] = 1.0;
            vertices[5] = r;
            vertices[6] = -1.0;
            vertices[10] = r;
            vertices[11] = -1.0;
            vertices[15] = r;
            vertices[16] = 1.0;
        } else {
            vertices[1] = 1.0;
            vertices[6] = -1.0;
            vertices[11] = -1.0;
            vertices[16] = 1.0;
        }

        glBufferSubData( GL_ARRAY_BUFFER, 0, 20 * sizeof( float ), vertices );
    }

    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, textures[0] );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, Frame->linesize[0] );

    if ( changed ) {
        glTexImage2D( GL_TEXTURE_2D,
                      0,
                      GL_RED,
                      Frame->width,
                      Frame->height,
                      0,
                      GL_RED,
                      GL_UNSIGNED_BYTE,
                      Frame->data[0] );
    } else {
        glTexSubImage2D( GL_TEXTURE_2D,
                         0,
                         0,
                         0,
                         Frame->width,
                         Frame->height,
                         GL_RED,
                         GL_UNSIGNED_BYTE,
                         Frame->data[0] );
    }

    glActiveTexture( GL_TEXTURE1 );
    glBindTexture( GL_TEXTURE_2D, textures[1] );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, Frame->linesize[1] );

    if ( changed ) {
        glTexImage2D( GL_TEXTURE_2D,
                      0,
                      GL_RED,
                      Frame->width / 2,
                      Frame->height / 2,
                      0,
                      GL_RED,
                      GL_UNSIGNED_BYTE,
                      Frame->data[1] );
    } else {
        glTexSubImage2D( GL_TEXTURE_2D,
                         0,
                         0,
                         0,
                         Frame->width / 2,
                         Frame->height / 2,
                         GL_RED,
                         GL_UNSIGNED_BYTE,
                         Frame->data[1] );
    }

    glActiveTexture( GL_TEXTURE2 );
    glBindTexture( GL_TEXTURE_2D, textures[2] );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, Frame->linesize[2] );
    if ( changed ) {
        glTexImage2D( GL_TEXTURE_2D,
                      0,
                      GL_RED,
                      Frame->width / 2,
                      Frame->height / 2,
                      0,
                      GL_RED,
                      GL_UNSIGNED_BYTE,
                      Frame->data[2] );
    } else {
        glTexSubImage2D( GL_TEXTURE_2D,
                         0,
                         0,
                         0,
                         Frame->width / 2,
                         Frame->height / 2,
                         GL_RED,
                         GL_UNSIGNED_BYTE,
                         Frame->data[2] );
    }

    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );

    glDrawElements( GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0 );
}