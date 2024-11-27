////////////////////////////////////////////////////////////////////////////////
// Copyright © 2024 Jon DuBois. Written with the assistance of GPT-4 et al.   //
////////////////////////////////////////////////////////////////////////////////



#include "Atlas.h" 

// DANGER this sets owns data to false, therefore the undelyinng data MUST NOT be destroyed BEFORE the copy
// while the programming is running. At exit cleanup, it shouldn't matter the order of deallocation.
tensor* copyTensor( const tensor* t ){
  tensor* ret = mem( 1, tensor );
  memcpy( ret, t, sizeof( tensor ) );
  ret->ownsData = false;
  return ret;    
}
// This converts a tensor to cpu memory.
void tensorToHostMemory( tensor* t ){
  if( !t->gpu )
    return;
  if( t == NULL )
    error( "%s", "Tensor is NULL in tensorToHostMemory." );

  f32* hostData = mem( t->size, f32 );
  f32* tempData = mem( t->tex.width * t->tex.height * 4, f32 ); // RGBA channels

  glBindFramebuffer( GL_FRAMEBUFFER, t->tex.framebuffer );
  glReadPixels( 0, 0, t->tex.width, t->tex.height, GL_RGBA, GL_FLOAT, tempData );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );

  memcpy( hostData, tempData, t->size * sizeof( f32 ) );

  unmem( tempData );

  if( t->ownsData ){
    if( t->tex.texture ){
      glDeleteTextures( 1, &t->tex.texture );
      t->tex.texture = 0;
    }
    if( t->tex.framebuffer ){
      glDeleteFramebuffers( 1, &t->tex.framebuffer );
      t->tex.framebuffer = 0;
    }
  }
  
  t->data = hostData;
  t->gpu = false;
  t->ownsData = true;
}
void tensorToGPUMemory( tensor* t ){
  if( t->gpu )
    return;
  if( t == NULL )
    error( "%s", "Tensor is NULL in tensorToGPUMemory." );

  // Calculate texture dimensions for GPU storage
  f32* tdata = t->data;
  u32 pixels = ( t->size + 3 ) / 4; // RGBA = 4 floats per pixel
  u32 twidth = ceilf( sqrtf( (f32)pixels ) );
  u32 theight = ( pixels + twidth - 1 ) / twidth;

  // Prepare padded data for texture upload
  f32* paddedData = mem( twidth * theight * 4, f32 );
  memset( paddedData, 0, twidth * theight * 4 * sizeof( f32 ) );
  memcpy( paddedData, t->data, t->size * sizeof( f32 ) );

  t->tex.width = twidth;
  t->tex.height = theight;
    
  glGenTextures( 1, &t->tex.texture );
  glBindTexture( GL_TEXTURE_2D, t->tex.texture );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, t->tex.width, t->tex.height, 0, GL_RGBA, GL_FLOAT, NULL );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

  glGenFramebuffers( 1, &t->tex.framebuffer );
  glBindFramebuffer( GL_FRAMEBUFFER, t->tex.framebuffer );
  glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->tex.texture, 0 );

  if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
    error( "%s", "Framebuffer is not complete." );

  glBindFramebuffer( GL_FRAMEBUFFER, 0 );


  glBindTexture( GL_TEXTURE_2D, t->tex.texture );
  glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, t->tex.width, t->tex.height, GL_RGBA, GL_FLOAT, paddedData );
  glBindTexture( GL_TEXTURE_2D, 0 );

  unmem( paddedData );
  if( t->ownsData ){
    unmem( tdata );
  }

  t->gpu = true;
  t->ownsData = true;
}
// Warning! this takes ownership of data and will deallocate it.
tensor* newTensor( u32 rank, u32* shape, f32* data ){
  tensor* ret = mem( 1, tensor );

  if( rank > 4 )
    error( "%s", "Rank exceeds maximum of 4." );

  // Initialize basic properties
  ret->rank = rank;
  ret->size = 1;
  ret->offset = 0;
  ret->ownsData = true;
  for( u32 i = 0; i < rank; ++i ){
    ret->shape[ i ] = shape[ i ];
    ret->strides[ rank - i - 1 ] = ret->size;
    ret->size *= shape[ rank - i - 1 ];
  }
  for( u32 i = rank; i < 4; ++i ){
    ret->shape[ i ] = 1;
    ret->strides[ i ] = 1;
  }

  ret->gpu = false;
  
  if( !data )
    error( "%s", "Null data!" );
  ret->data = data;
  
  return ret;
}
void deleteTensor( tensor* t ){
  if( t == NULL )
    return;
  if( t->ownsData ){
    if( t->gpu ){
      if( t->tex.texture ){
	glDeleteTextures( 1, &t->tex.texture );
	t->tex.texture = 0;
      }
      if( t->tex.framebuffer ){
	glDeleteFramebuffers( 1, &t->tex.framebuffer );
	t->tex.framebuffer = 0;
      }
    } else{
      unmem( t->data );
    }
  }
  unmem( t );
}
compute* makeCompute( const char* glsl, u32 argCount ){ // TODO make compute struct that stores buffer, program, uniform locs, etc.
  // Vertex shader source (simple pass-through)
  compute* ret = mem( 1, compute );
  const char* vertexShaderSource = "\
    #version 300 es\n\
    precision highp float;\n\
    in vec2 _a_position;\n\
    void main(){\n\
      gl_Position = vec4( _a_position, 0.0, 1.0);\n\
    }\n\
  ";

  // Fragment shader template
  const char* fragmentShaderTemplate = "\
    #version 300 es\n\
    precision highp float;\n\
    out vec4 _a_fragColor;\n\
    uniform vec2 _a_dims; // Texture dimensions\n\
    uniform vec4 _a_strides; // Tensor shape\n\
    vec4 _a_toTensorIndices( float i ) {\n\
      vec4 ret;\n\
      ret.x = floor(i / _a_strides.x);\n\
      i -= ret.x * _a_strides.x;\n\
      ret.y = floor(i / _a_strides.y);\n\
      i -= ret.y * _a_strides.y;\n\
      ret.z = floor(i / _a_strides.z);\n\
      i -= ret.z * _a_strides.z;\n\
      ret.w = i;\n\
      return ret;\n\
    }\n\
    void main() {\n\
      float i = ( ( gl_FragCoord.x - 0.5 ) + ( gl_FragCoord.y - 0.5 ) * _a_dims.x ) * 4.0 + 0.5;\n\
      vec4 t = _a_toTensorIndices( i );\n\
      float _a_r = (%s); ++i;\n\
      t = _a_toTensorIndices( i );\n\
      float _a_g = (%s); ++i;\n\
      t = _a_toTensorIndices( i );\n\
      float _a_b = (%s); ++i;\n\
      t = _a_toTensorIndices( i );\n\
      float _a_a = (%s);\n\
      _a_fragColor = vec4( _a_r, _a_g, _a_b, _a_a );\n\
    }\n\
  ";

  // Buffer to hold the final fragment shader source
  u32 bufsize = 65536;
  char* fragmentShaderSource = mem( bufsize, char ); // Adjust size as needed
  int len = snprintf( fragmentShaderSource, bufsize, fragmentShaderTemplate, glsl, glsl, glsl, glsl );
  if( len < 0 || len >= bufsize )
    error( "%s", "Shader source exceeds buffer size." );

  // Compile the vertex shader
  GLuint vertexShader = glCreateShader( GL_VERTEX_SHADER );
  glShaderSource( vertexShader, 1, &vertexShaderSource, NULL );
  glCompileShader( vertexShader );

  // Check for vertex shader compilation errors
  GLint status;
  glGetShaderiv( vertexShader, GL_COMPILE_STATUS, &status );
  if( status != GL_TRUE ){
    static char msg[ 2048 ];
    char log[ 1024 ];
    glGetShaderInfoLog( vertexShader, sizeof( log ), NULL, log );
    snprintf( msg, sizeof( msg ), "Vertex shader compilation failed: %s", log );
    glDeleteShader( vertexShader );
    error( "%s", msg );
  }

  // Compile the fragment shader
  GLuint fragmentShader = glCreateShader( GL_FRAGMENT_SHADER );
  const char* p = fragmentShaderSource;
  glShaderSource( fragmentShader, 1, &p, NULL );
  glCompileShader( fragmentShader );
  unmem( fragmentShaderSource );
  
  // Check for fragment shader compilation errors
  glGetShaderiv( fragmentShader, GL_COMPILE_STATUS, &status );
  if( status != GL_TRUE ){
    static char msg[ 512 ];
    char log[ 512 ];
    glGetShaderInfoLog( fragmentShader, sizeof( log ), NULL, log );
    snprintf( msg, sizeof( msg ), "Fragment shader compilation failed: %s", log );
    glDeleteShader( fragmentShader );
    glDeleteShader( vertexShader );
    error( "%s", msg );
  }

  // Create the program and attach both shaders
  ret->program = glCreateProgram();
  glAttachShader( ret->program, vertexShader );
  glAttachShader( ret->program, fragmentShader );

  // Bind attribute locations (if any)
  glBindAttribLocation( ret->program, 0, "a_position" );

  // Link the program
  glLinkProgram( ret->program );

  // Check for linking errors
  glGetProgramiv( ret->program, GL_LINK_STATUS, &status );
  if( status != GL_TRUE ){
    static char msg[ 512 ];
    char log[ 512 ];
    glGetProgramInfoLog( ret->program, sizeof( log ), NULL, log );
    snprintf( msg, sizeof( msg ), "Program linking failed: %s", log );
    glDeleteProgram( ret->program );
    glDeleteShader( vertexShader );
    glDeleteShader( fragmentShader );
    error( "%s", msg );
  }

  // Set uniform locations and VBO.
  ret->dimsLocation = glGetUniformLocation( ret->program, "_a_dims" );
  ret->stridesLocation = glGetUniformLocation( ret->program, "_a_strides" );
  
  f32 vertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f  
  };

  glGenBuffers( 1, &ret->VBO );
  glBindBuffer( GL_ARRAY_BUFFER, ret->VBO );
  glBufferData( GL_ARRAY_BUFFER, sizeof( vertices ), vertices, GL_STATIC_DRAW );

  // Cleanup shaders (they're no longer needed once the program is linked)
  glDeleteShader( vertexShader );
  glDeleteShader( fragmentShader );

  return ret;
}
void deleteCompute( compute* i ){
  glDeleteProgram( i->program );
  glDeleteBuffers( 1, &i->VBO );
  unmem( i );
}
tensor* newTensorInitialized( u32 rank, u32* shape, const compute* compute ){
  tensor* ret = mem( 1, tensor );

  if( rank > 4 )
    error( "%s", "Rank exceeds maximum of 4." );

  // Initialize basic properties
  ret->rank = rank;
  ret->size = 1;
  ret->offset = 0;
  ret->ownsData = true;
  ret->gpu = true;
  for( u32 i = 0; i < rank; ++i ){
    ret->shape[ i ] = shape[ i ];
    ret->strides[ rank - i - 1 ] = ret->size;
    ret->size *= shape[ rank - i - 1 ];
  }
  for( u32 i = rank; i < 4; ++i ){
    ret->shape[ i ] = 1;
    ret->strides[ i ] = 1;
  }

  // Compute the smallest square dimensions
  u32 pixels = ( ret->size + 3 ) / 4;
  ret->tex.width = (u32)ceilf( sqrtf( (f32)pixels ) );
  ret->tex.height = ( pixels + ret->tex.width - 1 ) / ret->tex.width;

  // Create OpenGL texture
  glGenTextures( 1, &ret->tex.texture );
  glBindTexture( GL_TEXTURE_2D, ret->tex.texture );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, ret->tex.width, ret->tex.height, 0, GL_RGBA, GL_FLOAT, NULL );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

  // Create framebuffer
  glGenFramebuffers( 1, &ret->tex.framebuffer );
  glBindFramebuffer( GL_FRAMEBUFFER, ret->tex.framebuffer );
  glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ret->tex.texture, 0 );

  if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
    error( "%s", "Framebuffer is not complete." );


  // Use the compute program to render to the texture
  glViewport( 0, 0, ret->tex.width, ret->tex.height );

  glUseProgram( compute->program );

  glUniform2f( compute->dimsLocation, (f32)ret->tex.width, (f32)ret->tex.height );
  glUniform4f( compute->stridesLocation, (f32)ret->strides[ 0 ], (f32)ret->strides[ 1 ],
	       (f32)ret->strides[ 2 ], (f32)ret->strides[ 3 ] );

  glBindBuffer( GL_ARRAY_BUFFER, compute->VBO );

  // Enable the vertex attribute and set up the pointer
  glEnableVertexAttribArray( 0 ); // Assuming attribute location 0 for a_position
  glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 0, (void* )0 );

  // Draw the quad
  glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );

  glBindTexture( GL_TEXTURE_2D, 0 );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );
  return ret;
}

void push( tensorStack* ts, tensor* t ){
  // Grow stack if necessary. 
  if( ts->top >= ts->size ){
    ts->size *= 2;
    tensor** ns = mem( ts->size, tensor* );
    memcpy( ns, ts->stack, sizeof( tensor* ) * ( ts->top - 1 ) );
    unmem( ts->stack );
    ts->stack = ns;
  }
  ts->stack[ ts->top++ ] = t;
}


void pop( tensorStack* ts ){
  if( !ts->top )
    error( "%s", "Atempt to pop an empty stack!" );
  deleteTensor( ts->stack[ --ts->top ] );
}

tensorStack* newStack( void ){
  tensorStack* ret = mem( 1, tensorStack );
  ret->size = 256;
  ret->stack = mem( ret->size, tensor );
  ret->top = 0;
  return ret;
}

void deleteStack( tensorStack* ts ){
  for( u32 i = 0; i < ts->top; ++i )
    deleteTensor( ts->stack[ i ] );
  unmem( ts->stack );
  unmem( ts );
}
  
void printStack( tensorStack* ts ){
  for( u32 i = ts->top - 1; i < ts->top; --i ){
    tensor* t = ts->stack[ i ];
    printf( "Tensor %u\n", i );
    printf( "Shape:" );
    for( u32 j = 0; j < t->rank; ++j )
      printf( " %u", t->shape[ j ] );
    printf( "\nStrides:" );
    for( u32 j = 0; j < t->rank; ++j )
      printf( " %i", t->strides[ j ] );
    if( t->size < 256 ){
      char* fd = formatTensorData( t );
      printf( "\n%s\n\n", fd );
      unmem( fd );
    } else
      printf( "\n[large tensor]\n\n" );
  }
}

void tensorReshapeHelper( tensor* t, u32 newRank, u32* newShape ){
  if( !t || !newShape || !newRank || !t->rank )
    error( "%s", "Invalid tensor or shape." );
  u32 newSize = 1;
  for( u32 i = 0; i < newRank; ++i )
    newSize *= newShape[ i ];
  if( newSize != t->size )
    error( "%s", "New shape size does not match tensor size." );
  
  memcpy( t->shape, newShape, sizeof( u32 ) * newRank );
  u32 size = 1;
  for( int i = newRank - 1; i >= 0; --i ){
    t->strides[ i ] = size;
    size *= newShape[ i ];
  }

  t->rank = newRank;
}
void tensorReshape( tensorStack* ts, u32 index, u32 newRank, u32* newShape ){
  tensorReshapeHelper( ts->stack[ index ], newRank, newShape );
}

void tensorTransposeHelper( tensor* t, u32 axis1, u32 axis2 ){
  if( axis1 >= t->rank || axis2 >= t->rank )
    error( "%s", "Invalid axes in transpose." );
  u32 tmp = t->shape[ axis1 ];
  t->shape[ axis1 ] = t->shape[ axis2 ];
  t->shape[ axis2 ] = tmp;
  tmp = t->strides[ axis1 ];
  t->strides[ axis1 ] = t->strides[ axis2 ];
  t->strides[ axis2 ] = tmp;
}
void tensorTranspose( tensorStack* ts, u32 index, u32 axis1, u32 axis2 ){
  tensorTransposeHelper( ts->stack[ index ], axis1, axis2 );
}

void tensorReverseHelper( tensor* t, u32 axis ){
  if( axis > 3 )
    error( "%s", "Invalid axis in reverse." );
  t->offset += t->strides[ axis ] * ( t->shape[ axis ] - 1 );
  t->strides[ axis ] = -t->strides[ axis ];
}
void tensorReverse( tensorStack* ts, u32 index, u32 axis ){
  tensorReverseHelper( ts->stack[ index ], axis );
}
