////////////////////////////////////////////////////////////////////////////////
// Copyright © 2024 Jon DuBois. Written with the assistance of o1-preview etc.//
////////////////////////////////////////////////////////////////////////////////


typedef struct{
  u32 rank;              // Rank of the tensor (0 to 4)
  u32 size;              // Total number of elements
  u32 shape[4];          // Dimensions of the tensor
  s32 strides[4];        // Strides for indexing
  u32 offset;
  u32 width, height;
  GLuint texture;        // OpenGL texture for reading/writing operations
  GLuint framebuffer;    // Framebuffer for rendering into the texture
  bool ownsData;
} tensor;

typedef struct{
  GLuint program;
  GLuint dimsLocation;
  GLuint stridesLocation;
  GLuint VBO;
} initializer;

typedef struct{
  u32 size;
  tensor** stack;
  u32 top;
} tensorStack;


f32* tensorToHostMemory( const tensor* t );
tensorStack* newStack( void );
tensor* newTensor( u32 rank, u32* shape, f32* data );
initializer* makeInitializer( const char* glsl );
void deleteInitializer( initializer* i );
tensor* newTensorInitialized( u32 rank, u32* shape, const initializer* initializer );
void deleteStack( tensorStack* ts );
void push( tensorStack* ts, tensor* t );
void tensorReshape( tensorStack* ts, u32 index, u32 newRank, u32* newShape );
void tensorTranspose( tensorStack* ts, u32 index, u32 axis1, u32 axis2 );
void tensorReverse( tensorStack* ts, u32 index, u32 axis );
void pop( tensorStack* ts );
// Functions for printing tensors.
char* formatTensorData( const tensor* t );
void printStack( const tensorStack* ts );


  
