////////////////////////////////////////////////////////////////////////////////
// Copyright © 2024 Jon DuBois. Written with the assistance of GPT-4 et al.   //
////////////////////////////////////////////////////////////////////////////////

#include "Atlas.h"

void skipWhitespace( const char** str ){
  while( isspace( **str ) )
    ( *str )++;
}
void parseTensorRecursive(
  const char** str, u32 currentDim, u32* shape, float* data, u32* dataIndex ){
  skipWhitespace( str );
  if( **str != '[' )
    error( "%s", "Expected '[' to start tensor definition." );
  ( *str )++;
  skipWhitespace( str );

  u32 dim_size = 0;
  while( **str != ']' && **str != '\0' ){
    if( **str == '[' ){
      if( currentDim + 1 >= 4 )
        error( "%s", "Tensor exceeds maximum supported dimensions (4D)." );
      parseTensorRecursive( str, currentDim + 1, shape, data, dataIndex );
      dim_size++;
    } else {
      float num;
      int charsread;
      if( sscanf( *str, "%f%n", &num, &charsread ) != 1 )
        error( "%s", "Failed to parse number in tensor." );
      *str += charsread;
      data[ *dataIndex ] = num;
      ( *dataIndex )++;
      dim_size++;
    }

    skipWhitespace( str );
  }

  if( **str != ']' )
    error( "%s", "Expected ']' to close tensor definition." );

  ( *str )++;
  skipWhitespace( str );

  // Update shape
  if( !shape[ currentDim ] )
    shape[ currentDim ] = dim_size;
  else if( shape[ currentDim ] != dim_size )
    error( "%s", "Inconsistent tensor shape detected." );
}
// Function to determine shape
void determineShape( const char** s, u32 currentDim, u32* tempShape ){
  skipWhitespace( s );
  if( **s != '[' )
    error( "%s", "Expected '[' to start tensor definition." );
  ( *s )++;
  skipWhitespace( s );

  u32 dim_size = 0;

  while( **s != ']' && **s != '\0' ){
    if( **s == '[' ){
      // Nested tensor
      determineShape( s, currentDim + 1, tempShape );
      dim_size++;
    } else {
      // Parse a number
      float num;
      int charsread;
      if( sscanf( *s, "%f%n", &num, &charsread ) != 1 )
        error( "%s", "Failed to parse number in tensor." );
      *s += charsread;
      dim_size++;
    }

    skipWhitespace( s );
  }

  if( **s != ']' )
    error( "%s", "Expected ']' to close tensor definition." );
  ( *s )++;  // Skip ']'
  skipWhitespace( s );

  // Update shape
  if( !tempShape[ currentDim ] )
    tempShape[ currentDim ] = dim_size;
  else if( tempShape[ currentDim ] != dim_size )
    error( "%s", "Inconsistent tensor shape detected." );
}

static tensor* parseTensor( const char* command ){
  u32 shape[ 4 ] = { 0, 0, 0, 0 };  // Initialize shape to zero
  float* tempData = NULL;
  u32 dataCount = 0;

  // To determine the number of elements, we'll need to parse the tensor
  // First, we make a pass to determine the shape and total elements

  // Clone the string to parse
  char* clone = mem( strlen( command ) + 1, char );
  strcpy( clone, command );
  const char* parsePtr = clone;

  u32 tempShape[ 4 ] = { 0, 0, 0, 0 };

  determineShape( &parsePtr, 0, tempShape );

  // Now, determine the rank by finding the deepest non-zero dimension
  u32 rank = 0;
  for( u32 i = 0; i < 4; ++i ){
    if( tempShape[ i ] > 0 )
      rank = i + 1;
  }

  // Validate that all deeper dimensions are set
  for( u32 i = 0; i < rank; ++i ){
    if( tempShape[ i ] == 0 )
      error( "%s", "Incomplete tensor shape definition." );
  }

  u32 totalElements = 1;
  for( u32 i = 0; i < rank; ++i )
    totalElements *= tempShape[ i ];

  tempData = mem( totalElements, f32 );

  parsePtr = clone;
  parseTensorRecursive( &parsePtr, 0, tempShape, tempData, &dataCount );

  skipWhitespace( &parsePtr );
  if( *parsePtr != '\0' )
    error( "%s", "Unexpected characters after tensor definition." );

  if( dataCount != totalElements )
    error( "%s", "Mismatch in expected and actual number of tensor elements." );

  memcpy( shape, tempShape, sizeof( shape ) );

  unmem( clone );

  tensor* t = mem( 1, tensor );
  t->rank = rank;
  t->size = 1;
  for( u32 i = 0; i < 4; ++i )
    t->strides[ i ] = t->shape[ i ] = 1;
  for( int i = rank - 1; i >= 0; --i ){
    t->strides[ i ] = t->size;
    t->shape[ i ] = shape[ i ];
    t->size *= shape[ i ];
  }
  t->offset = 0;
  t->gpu = false;
  t->ownsData = true;
  t->data = tempData;

  return t;
}
// Function to remove all '//' comments from the program string
void removeComments( char* prog ){
  char* src = prog;
  char* dst = prog;

  while( *src != '\0' ){
    if( src[ 0 ] == '/' && src[ 1 ] == '/' ){
      // Skip characters until the end of the line
      src += 2;
      while( *src != '\n' && *src != '\0' ){
        src++;
      }
    } else {
      *dst++ = *src++;
    }
  }
  *dst = '\0';  // Null-terminate the modified string
}
// This adds a compute statement to p and returns its index.
u32 addCompute( program* p,
                const char* uniforms,
                const char* glslpre,
                const char* glsl,
                u32 argCount,
                u32 retCount ){
  if( p->numComputes >= p->computeStackSize ){
    p->computeStackSize *= 2;
    compute** tp = mem( p->computeStackSize, compute* );
    memcpy( tp, p->computes, sizeof( compute* ) * p->numComputes );
    unmem( p->computes );
    p->computes = tp;
  }
  p->computes[ p->numComputes ] =
    makeCompute( p, uniforms, glslpre, glsl, argCount, retCount );
  return p->numComputes++;
}
char* getNextLine( char** str ){
  if( *str == NULL || **str == '\0' )
    return NULL;

  char* start = *str;
  char* end = strchr( start, '\n' );

  if( end != NULL ){
    *end = '\0';     // Replace '\n' with '\0'
    *str = end + 1;  // Move to the next line
  } else {
    *str = NULL;  // No more lines
  }

  return start;
}
void trimWhitespace( char** str ){
  char* start = *str;
  char* end;

  while( isspace( *start ) )
    start++;

  if( *start == '\0' ){
    *str = start;
    return;
  }

  end = start + strlen( start ) - 1;

  while( end > start && isspace( *end ) ){
    *end = '\0';
    --end;
  }

  *str = start;
}
void addStep( program* p, u32 linenum, u32 commandnum, char* command ){
  if( !command )
    return;
  trimWhitespace( &command );
  if( !*command )
    return;
  if( p->numSteps >= p->stepStackSize ){
    p->stepStackSize *= 2;
    step* tp = mem( p->stepStackSize, step );
    memcpy( tp, p->steps, sizeof( step ) * p->numSteps );
    unmem( p->steps );
    p->steps = tp;
  }
  step* curStep = &( p->steps[ p->numSteps ] );
  ++p->numSteps;

  if( !strncmp( command, "l'", 2 ) ){  // Label
    char* starti = command + 2;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s", linenum, commandnum, "Empty label." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in label." );
    char* label = mem( 1 + endi - starti, char );
    memcpy( label, starti, endi - starti );
    label[ endi - starti ] = '\0';
    --p->numSteps;
    if( *( endi + 1 ) )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Extra characters after label." );
    if( trieSearch( p->labels, label, NULL ) )
      error( "Line %u, command %u: duplicate label '%s'",
             linenum,
             commandnum,
             label );
    trieInsert( p->labels, label, p->numSteps );
    // dbg( "Linenum %u commandnum %u: label: %s\n", linenum, commandnum, label
    // );
    unmem( label );

  } else if( !strncmp( command, "set'", 4 ) ){  // Set
    char* starti = command + 4;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Empty name in set statement." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in set statement." );
    char* varName = mem( 1 + endi - starti, char );
    memcpy( varName, starti, endi - starti );
    varName[ endi - starti ] = '\0';
    char* sizep = endi + 1;
    u32 varSize;
    int charsread;
    if( sscanf( sizep, "%u%n", &varSize, &charsread ) == 1 &&
        !sizep[ charsread ] ){
      curStep->type = SET;
      curStep->var.name = varName;
      curStep->var.size = varSize;
      if( !varSize || ( varSize > 4 && varSize != 16 ) )
        error( "%s", "Invalid var size in set statement." );
      // dbg( "Linenum %u commandnum %u: set '%s' of size %u.\n",
      // linenum, commandnum, varName, varSize );
    } else
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Malformed set statement." );
    // dbg( "Linenum %u commandnum %u: set var %s\n", linenum, commandnum,
    // varName );

  } else if( !strncmp( command, "get'", 4 ) ){  // Get
    char* starti = command + 4;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Empty name in get statement." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in get statement." );
    char* varName = mem( 1 + endi - starti, char );
    memcpy( varName, starti, endi - starti );
    varName[ endi - starti ] = '\0';
    if( *( endi + 1 ) )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Extra characters after get statement." );
    curStep->type = GET;
    curStep->var.name = varName;
    // dbg( "Linenum %u commandnum %u: get var %s\n", linenum, commandnum,
    // varName );

  } else if( !strncmp( command, "if'", 3 ) ){  // If
    char* starti = command + 3;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error(
        "Line %u, command %u: %s", linenum, commandnum, "Empty if statement." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in if statement." );
    char* branchName = mem( 1 + endi - starti, char );
    memcpy( branchName, starti, endi - starti );
    branchName[ endi - starti ] = '\0';
    curStep->type = IF;
    curStep->branchName = branchName;
    if( *( endi + 1 ) )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Extra characters after if statement." );
    // dbg( "Linenum %u commandnum %u: if to %s\n", linenum, commandnum,
    // branchName );

  } else if( !strncmp( command, "ifn'", 4 ) ){  // Ifn
    char* starti = command + 4;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Empty ifn statement." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in ifn statement." );
    char* branchName = mem( 1 + endi - starti, char );
    memcpy( branchName, starti, endi - starti );
    branchName[ endi - starti ] = '\0';
    curStep->type = IFN;
    curStep->branchName = branchName;
    if( *( endi + 1 ) )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Extra characters after ifn statement." );
    // dbg( "Linenum %u commandnum %u: ifn to %s\n", linenum, commandnum,
    // branchName );

  } else if( !strncmp( command, "img'", 4 ) ){  // img
    char* starti = command + 4;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Empty img statement." );
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in img statement." );
    char* imgName = mem( 1 + endi - starti, char );
    memcpy( imgName, starti, endi - starti );
    imgName[ endi - starti ] = '\0';
    curStep->type = TENSOR;
    curStep->tensor = tensorFromImageFile( imgName );
    unmem( imgName );
    if( *( endi + 1 ) )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Extra characters after img statement." );
    // dbg( "Linenum %u commandnum %u: img %s\n", linenum, commandnum,
    // imgName );

  } else if( !strncmp( command, "c'", 2 ) ){  // Compute
    char* starti = command + 2;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in compute statement pre block." );
    char* pre = mem( 1 + endi - starti, char );
    memcpy( pre, starti, endi - starti );
    // Replace \ with ;
    for( u32 i = 0; i < endi - starti; ++i )
      if( pre[ i ] == '\\' )
        pre[ i ] = ';';
    pre[ endi - starti ] = '\0';
    starti = endi + 1;
    ++endi; // now get compute statements
    while( *endi && *endi != '\'' )
      endi++;
    if( *endi != '\'' )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Unmatched quote in compute statement." );
    char* comp = mem( 1 + endi - starti, char );
    memcpy( comp, starti, endi - starti );
    // Replace \ with ;
    for( u32 i = 0; i < endi - starti; ++i )
      if( comp[ i ] == '\\' )
        comp[ i ] = ';';
    comp[ endi - starti ] = '\0';
    
    char* sizep = endi + 1;
    u32 argCount, retCount;
    int charsread;
    if( sscanf( sizep, "%u%u%n", &argCount, &retCount, &charsread ) == 2 &&
        !sizep[ charsread ] ){
      curStep->type = COMPUTE;
      curStep->toCompute.glslpre = pre;
      curStep->toCompute.glsl = comp;
      curStep->toCompute.retCount = retCount;
      curStep->toCompute.argCount = argCount;
      if( argCount > 4 )
        error(
          "%s",
          "Compute created with more than 4 arguments. The maximum is 4." );
      // dbg( "Linenum %u commandnum %u: compute '%s' on %u arguments.\n",
      // linenum, commandnum, comp, argCount );
    } else
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Malformed compute statement." );

  } else if( !strcmp( command, "first" ) ){  // First
    curStep->type = FIRST;
    // dbg( "Linenum %u commandnum %u: first\n", linenum, commandnum );

  } else if( !strcmp( command, "last" ) ){  // Last
    curStep->type = LAST;
    // dbg( "Linenum %u commandnum %u: last\n", linenum, commandnum );

  } else if( !strcmp( command, "+" ) ){  // Add
    curStep->type = ADD;
    // dbg( "Linenum %u commandnum %u: add\n", linenum, commandnum );

  } else if( !strcmp( command, "-" ) ){  // Sub
    curStep->type = SUB;
    // dbg( "Linenum %u commandnum %u: sub\n", linenum, commandnum );

  } else if( !strcmp( command, "*" ) ){  // Mul
    curStep->type = MUL;
    // dbg( "Linenum %u commandnum %u: mul\n", linenum, commandnum );

  } else if( !strcmp( command, "/" ) ){  // Div
    curStep->type = DIV;
    // dbg( "Linenum %u commandnum %u: div\n", linenum, commandnum );

  } else if( !strcmp( command, "^" ) ){  // Exp
    curStep->type = POW;
    // dbg( "Linenum %u commandnum %u: exp\n", linenum, commandnum );

  } else if( !strcmp( command, "r" ) ){  // Reverse
    curStep->type = REVERSE;
    // dbg( "Linenum %u commandnum %u: reverse\n", linenum, commandnum );

  } else if( !strcmp( command, "cat" ) ){  // Concatenate
    curStep->type = CAT;
    // dbg( "Linenum %u commandnum %u: cat\n", linenum, commandnum );

  } else if( !strcmp( command, "pop" ) ){  // Pop
    curStep->type = POP;
    // dbg( "Linenum %u commandnum %u: pop\n", linenum, commandnum );

  } else if( !strcmp( command, "rep" ) ){  // Rep
    curStep->type = REPEAT;
    // dbg( "Linenum %u commandnum %u: repeat\n", linenum, commandnum );

  } else if( !strcmp( command, "shape" ) ){  // Shape
    curStep->type = SHAPE;
    // dbg( "Linenum %u commandnum %u: shape\n", linenum, commandnum );

  } else if( !strcmp( command, "dup" ) ){  // Dup
    curStep->type = DUP;
    // dbg( "Linenum %u commandnum %u: dup\n", linenum, commandnum );

  } else if( !strcmp( command, "s" ) ){  // Slice
    curStep->type = SLICE;
    // dbg( "Linenum %u commandnum %u: slice\n", linenum, commandnum );

  } else if( !strcmp( command, "size" ) ){  // Top
    curStep->type = TOP;
    // dbg( "Linenum %u commandnum %u: top\n", linenum, commandnum );

  } else if( !strcmp( command, "return" ) ){  // Return
    curStep->type = RETURN;
    // dbg( "Linenum %u commandnum %u: return\n", linenum, commandnum );

  } else if( !strcmp( command, "input" ) ){  // Input; 3 axes and three buttons
    curStep->type = GETINPUT;
    // dbg( "Linenum %u commandnum %u: input\n", linenum, commandnum );

  } else if( !strcmp( command, "windowSize" ) ){  // Window size
    curStep->type = WINDOWSIZE;
    // dbg( "Linenum %u commandnum %u: window size\n", linenum, commandnum );

  } else if( !strcmp( command, "t" ) ){  // Transpose
    curStep->type = TRANSPOSE;
    // dbg( "Linenum %u commandnum %u: transpose\n", linenum, commandnum );

  } else if( *command == '[' || *command == '.' ||
             isdigit( *command ) || *command == '\'' ){  // A tensor
    curStep->type = TENSOR;
    if( *command == '\'' ){
      char* starti = command + 1;
      char* endi = starti;
      while( *endi && *endi != '\'' )
	endi++;
      if( endi == starti )
	error( "Line %u, command %u: %s",
	       linenum,
	       commandnum,
	       "Empty string statement." );
      if( *endi != '\'' )
	error( "Line %u, command %u: %s",
	       linenum,
	       commandnum,
	       "Unmatched quote in string statement." );
      char* str = mem( 1 + endi - starti, char );
      memcpy( str, starti, endi - starti );
      str[ endi - starti ] = '\0';
      curStep->tensor = tensorFromString( str );
      unmem( str );
      if( *( endi + 1 ) )
	error( "Line %u, command %u: %s",
	       linenum,
	       commandnum,
	       "Extra characters after string statement." );
      // dbg( "Linenum %u commandnum %u: string %s\n", linenum, commandnum,
      // string );
    } else{
      char* tp = command;
      f32 scalar;
      int charsread;
      if( sscanf( tp, "%f%n", &scalar, &charsread ) == 1 && !tp[ charsread ] ){
	curStep->tensor = mem( 1, tensor );
	curStep->tensor->size = 1;
	for( u32 i = 0; i < 4; ++i )
	  curStep->tensor->shape[ i ] = curStep->tensor->strides[ i ] = 1;
	curStep->tensor->data = mem( 1, f32 );
	*curStep->tensor->data = scalar;
	curStep->tensor->ownsData = true;
      } else
	curStep->tensor = parseTensor( command );
    }
    // dbg( "Linenum %u commandnum %u: tensor\n", linenum, commandnum );

  } else if( !strcmp( command, "print" ) ){  // Print
    curStep->type = PRINT;
    // dbg( "Linenum %u commandnum %u: print\n", linenum, commandnum );

  } else if( !strcmp( command, "quit" ) ){  // Quit
    curStep->type = QUIT;
    // dbg( "Linenum %u commandnum %u: quit\n", linenum, commandnum );

  } else {  // Call
    char* starti = command;
    char* endi = starti;
    while( *endi && *endi != '\'' )
      endi++;
    if( endi == starti )
      error( "Line %u, command %u: %s",
             linenum,
             commandnum,
             "Empty call statement." );
    if( *endi == '\'' )
      error( "Line %u, command %u: %s", linenum, commandnum, "Quote error." );
    char* branchName = mem( 1 + endi - starti, char );
    memcpy( branchName, starti, endi - starti );
    branchName[ endi - starti ] = '\0';
    curStep->type = CALL;
    curStep->branchName = branchName;
    // dbg( "Linenum %u commandnum %u: call to %s\n", linenum, commandnum,
    // branchName );
  }
}
// Modifies prog.
program* newProgram( char* prog ){
  removeComments( prog );
  program* ret = mem( 1, program );
  ret->computes = mem( initSize, compute* );
  ret->numComputes = 0;
  ret->computeStackSize = initSize;
  ret->steps = mem( initSize, step );
  ret->numSteps = 0;
  ret->stepStackSize = initSize;
  ret->labels = newTrieNode( NULL, 0 );
  ret->vars = newTrieNode( NULL, 0 );
  ret->numReturns = 0;
  ret->returns = mem( initSize, step );
  ret->returnStackSize = initSize;

  // Step 1: Remove all comments from the program
  removeComments( prog );

  // Step 2: Initialize parsing pointers
  char* ptr = prog;
  u32 linenum = 1;
  u32 commandnum = 1;

  while( *ptr != '\0' ){
    // Find the next semicolon
    char* semicolon = strchr( ptr, ';' );
    if( !semicolon ){
      semicolon =
        ptr + strlen( ptr );  // Point to the end if no semicolon found
    }

    // Calculate the length of the current command
    size_t cmd_length = semicolon - ptr;
    if( cmd_length == 0 ){
      // Empty command (e.g., consecutive semicolons), skip
      ptr = ( *semicolon == ';' ) ? semicolon + 1 : semicolon;
      if( *semicolon == '\n' )
        linenum++;
      continue;
    }

    // Extract the command into a temporary buffer
    char* command = mem( cmd_length + 1, char );
    strncpy( command, ptr, cmd_length );
    command[ cmd_length ] = '\0';

    // Add the command to the program steps
    addStep( ret, linenum, commandnum, command );
    unmem( command );

    // Update the parsing pointer
    ptr = ( *semicolon == ';' ) ? semicolon + 1 : semicolon;

    // Update line and command numbers
    for( size_t i = 0; i < cmd_length; ++i ){
      if( ptr[ -cmd_length + i ] == '\n' ){
        linenum++;
        commandnum = 1;
      }
    }
    if( *semicolon == ';' ){
      commandnum++;
    }
  }

  // First collect variables and craft the uniform block and the program vars.
  char* glslUniformBlock;
  {
    ret->numVars = 0;
    u32 nameslen = 0;
    u32 baselen = strlen( "uniform float %s;" ) + 30;
    for( u32 i = 0; i < ret->numSteps; ++i )
      if( ret->steps[ i ].type == SET ){
        nameslen += strlen( ret->steps[ i ].var.name ) + 2;
        ret->numVars++;
      }
    ret->varOffsets = mem( ret->numVars, u32 );
    ret->varSizes = mem( ret->numVars, u32 );
    u32 bufsize = baselen * ret->numVars + nameslen + 200;
    glslUniformBlock = mem( bufsize, u8 );
    char* p = glslUniformBlock;
    //    p += snprintf( p, bufsize - ( p - glslUniformBlock ), "layout(std140)
    //    uniform vars{\n" );
    ret->varNames = mem( ret->numVars, char* );
    ret->numVars = 0;
    u32 offset = 0;
    for( u32 i = 0; i < ret->numSteps; ++i ){
      if( ret->steps[ i ].type == SET ){
        u32 val;
        if( trieSearch( ret->vars, ret->steps[ i ].var.name, &val ) ){
          if( ret->steps[ i ].var.size != ret->varSizes[ val ] )
            error(
              "%s",
              "Incorrect size setting already set value. Size is static." );
          unmem( ret->steps[ i ].var.name );
          ret->steps[ i ].var.index = val;
        } else {
          trieInsert( ret->vars, ret->steps[ i ].var.name, ret->numVars );
          switch( ret->steps[ i ].var.size ){
          case 1:
            p += snprintf( p,
                           bufsize - ( p - glslUniformBlock ),
                           "uniform float %s;\n",
                           ret->steps[ i ].var.name );
            break;
          case 2:
            p += snprintf( p,
                           bufsize - ( p - glslUniformBlock ),
                           "uniform vec2 %s;\n",
                           ret->steps[ i ].var.name );
            break;
          case 3:
            p += snprintf( p,
                           bufsize - ( p - glslUniformBlock ),
                           "uniform vec3 %s;\n",
                           ret->steps[ i ].var.name );
            break;
          case 4:
            p += snprintf( p,
                           bufsize - ( p - glslUniformBlock ),
                           "uniform vec4 %s;\n",
                           ret->steps[ i ].var.name );
            break;
          case 16:
            p += snprintf( p,
                           bufsize - ( p - glslUniformBlock ),
                           "uniform mat4 %s;\n",
                           ret->steps[ i ].var.name );
            break;
          default:
            error( "%s", "Logic error in Atlas! My code is ass!" );
          }
          ret->varNames[ ret->numVars ] = ret->steps[ i ].var.name;
          ret->varOffsets[ ret->numVars ] = offset;
          ret->varSizes[ ret->numVars ] = ret->steps[ i ].var.size;
          if( ret->steps[ i ].var.size == 1 || ret->steps[ i ].var.size == 2 )
            offset += 2;
          else if( ret->steps[ i ].var.size == 3 ||
                   ret->steps[ i ].var.size == 4 )
            offset += 4;
          else
            offset += 16;
          ret->steps[ i ].var.index = ret->numVars;
          ++ret->numVars;
        }
      }
    }

    // p += snprintf( p, bufsize - ( p - glslUniformBlock ), "};\n" );
    ret->varBlock = mem( offset, f32 );
    // glGenBuffers( 1, &ret->ubo );
    // glBindBuffer( GL_UNIFORM_BUFFER, ret->ubo );
    // glBufferData( GL_UNIFORM_BUFFER, sizeof( f32 ) * offset, ret->varBlock,
    // GL_DYNAMIC_DRAW ); glBindBufferBase( GL_UNIFORM_BUFFER, 0, ret->ubo );
    // glBindBuffer( GL_UNIFORM_BUFFER, 0 );
    // glBindBufferBase( GL_UNIFORM_BUFFER, 0, 0 );
    // dbg( "Block %s, totsize %u", glslUniformBlock, offset );
  }

  // Second pass for ifs, ifns, calls, computes, and gets.
  for( u32 i = 0; i < ret->numSteps; ++i )
    if( ret->steps[ i ].type == IF || ret->steps[ i ].type == IFN ||
        ret->steps[ i ].type == CALL ){
      u32 jumpTo;
      if( !trieSearch( ret->labels, ret->steps[ i ].branchName, &jumpTo ) )
        error( "Statement with unknown label %s.", ret->steps[ i ].branchName );
      unmem( ret->steps[ i ].branchName );
      ret->steps[ i ].branch = jumpTo;
    } else if( ret->steps[ i ].type == GET ){
      u32 vi;
      if( !trieSearch( ret->vars, ret->steps[ i ].var.name, &vi ) )
        error( "%s %s.",
               "Attempt to get an an unknown variable",
               ret->steps[ i ].var.name );
      char* varName = ret->steps[ i ].var.name;
      ret->steps[ i ].var.index = vi;
      unmem( varName );
    } else if( ret->steps[ i ].type == COMPUTE ){
      char* glslpre = ret->steps[ i ].toCompute.glslpre;
      char* glsl = ret->steps[ i ].toCompute.glsl;
      ret->steps[ i ].compute =
        addCompute( ret,
                    glslUniformBlock,
                    glslpre, glsl,
                    ret->steps[ i ].toCompute.argCount,
                    ret->steps[ i ].toCompute.retCount );
      unmem( glsl );
      unmem( glslpre );
    }
  unmem( glslUniformBlock );
  return ret;
}
// Doesn't modify prog.
program* newProgramFromString( const char* prog, u32 len ){
  char* cp = mem( len + 3, char );
  strncpy( cp, prog, len );
  cp[ len ] = '\0';
  program* ret = newProgram( cp );
  unmem( cp );
  return ret;
}
program* newProgramFromFile( const char* filename ){
  FILE* file = fopen( filename, "rb" );

  if( !file )
    error( "%s %s.", "Failed to open file", filename );
  // Seek to the end of the file to determine its size
  if( fseek( file, 0, SEEK_END ) ){
    fclose( file );
    error( "%s", "Failed to seek file." );
  }
  long fileSize = ftell( file );
  if( fileSize == -1 ){
    fclose( file );
    error( "%s", "Failed to get file size." );
  }
  fseek( file, 0, SEEK_SET );
  char* buffer = mem( fileSize + 10, char );
  // Read the file contents into the buffer
  size_t bytesRead = fread( buffer, 1, fileSize, file );
  if( bytesRead != fileSize ){
    unmem( buffer );
    fclose( file );
    error( "%s", "Failed to read file." );
  }

  buffer[ fileSize ] = '\0';
  fclose( file );
  program* ret = newProgram( buffer );
  unmem( buffer );
  return ret;
}
void deleteProgram( program* p ){
  for( u32 i = 0; i < p->numComputes; ++i )
    deleteCompute( p->computes[ i ] );
  for( u32 i = 0; i < p->numSteps; ++i ){
    if( p->steps[ i ].type == TENSOR ){
      deleteTensor( p->steps[ i ].tensor );
      p->steps[ i ].tensor = NULL;
    }
  }
  deleteTrieNode( p->labels );
  for( u32 i = 0; i < p->numVars; ++i )
    unmem( p->varNames[ i ] );
  unmem( p->varNames );
  deleteTrieNode( p->vars );
  unmem( p->varBlock );
  unmem( p->varSizes );
  unmem( p->varOffsets );
  unmem( p->returns );
  unmem( p->computes );
  unmem( p->steps );
  unmem( p );
}
bool runProgram( tensorStack* ts, program* p ){
  CHECK_GL_ERROR();
  for( u32 i = 0; i < p->numSteps; ++i ){
    // dbg( "Step %u", i );
    step* s = p->steps + i;
    switch( s->type ){
    case WINDOWSIZE: {
      static const u32 wsshape[ 1 ] = { 2 };
      int windowWidth, windowHeight;
      SDL_GetWindowSize( window, &windowWidth, &windowHeight );
      f32* data = mem( 2, f32 );
      data[ 0 ] = windowWidth;
      data[ 1 ] = windowHeight;
      push( ts, newTensor( 1, wsshape, data ) );
      break;
    }
    case GETINPUT: {
      static const u32 wsshape[ 1 ] = { 6 };
      f32* data = mem( 6, f32 );
      int dx, dy;
      SDL_GetRelativeMouseState( &dx, &dy );  // Get mouse delta
      data[ 0 ] = dx;
      data[ 1 ] = dy;
#ifndef __EMSCRIPTEN__
      SDL_LockMutex( data_mutex );
#endif
      data[ 2 ] = mouseWheelDelta;
      mouseWheelDelta = 0;
#ifndef __EMSCRIPTEN__
      SDL_UnlockMutex( data_mutex );
#endif
      Uint32 buttons = SDL_GetMouseState( NULL, NULL );
      if( buttons & SDL_BUTTON( SDL_BUTTON_LEFT ) )
        data[ 3 ] = 1;
      else
        data[ 3 ] = 0;
      if( buttons & SDL_BUTTON( SDL_BUTTON_RIGHT ) )
        data[ 4 ] = 1;
      else
        data[ 4 ] = 0;
      if( buttons & SDL_BUTTON( SDL_BUTTON_MIDDLE ) )
        data[ 5 ] = 1;
      else
        data[ 5 ] = 0;

      push( ts, newTensor( 1, wsshape, data ) );
      break;
    }
    case POW: {
      if( ts->size < 2 )
        error( "%s", "Attempt to pow without enough arguments on the stack." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      tensorToHostMemory( ts->stack[ ts->size - 2 ] );
      takeOwnership( ts->stack[ ts->size - 2 ] );
      tensor* t1 = ts->stack[ ts->size - 1 ];
      tensor* t2 = ts->stack[ ts->size - 2 ];
      if( t1->rank != t2->rank || t1->shape[ 0 ] != t2->shape[ 0 ] ||
          t1->shape[ 1 ] != t2->shape[ 1 ] ||
          t1->shape[ 2 ] != t2->shape[ 2 ] || t1->shape[ 3 ] != t2->shape[ 3 ] )
        error( "%s", "Attempt to pow tensors with incompatible shapes." );
      for( u32 i0 = 0; i0 < t1->shape[ 0 ]; ++i0 )
        for( u32 i1 = 0; i1 < t1->shape[ 1 ]; ++i1 )
          for( u32 i2 = 0; i2 < t1->shape[ 2 ]; ++i2 )
            for( u32 i3 = 0; i3 < t1->shape[ 3 ]; ++i3 ){
              f32* offset1 = t1->data + t1->offset + i0 * t1->strides[ 0 ] +
                             i1 * t1->strides[ 1 ] + i2 * t1->strides[ 2 ] +
                             i3 * t1->strides[ 3 ];
              f32* offset2 = t2->data + t2->offset + i0 * t2->strides[ 0 ] +
                             i1 * t2->strides[ 1 ] + i2 * t2->strides[ 2 ] +
                             i3 * t2->strides[ 3 ];
              *offset2 = powf( *offset2, *offset1 );
            }

      pop( ts );
      // dbg( "%s", "pow" );
      break;
    }
    case ADD: {
      if( ts->size < 2 )
        error( "%s", "Attempt to add without enough arguments on the stack." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      tensorToHostMemory( ts->stack[ ts->size - 2 ] );
      takeOwnership( ts->stack[ ts->size - 2 ] );
      tensor* t1 = ts->stack[ ts->size - 1 ];
      tensor* t2 = ts->stack[ ts->size - 2 ];
      if( t1->rank != t2->rank || t1->shape[ 0 ] != t2->shape[ 0 ] ||
          t1->shape[ 1 ] != t2->shape[ 1 ] ||
          t1->shape[ 2 ] != t2->shape[ 2 ] || t1->shape[ 3 ] != t2->shape[ 3 ] )
        error( "%s", "Attempt to add tensors with incompatible shapes." );
      for( u32 i0 = 0; i0 < t1->shape[ 0 ]; ++i0 )
        for( u32 i1 = 0; i1 < t1->shape[ 1 ]; ++i1 )
          for( u32 i2 = 0; i2 < t1->shape[ 2 ]; ++i2 )
            for( u32 i3 = 0; i3 < t1->shape[ 3 ]; ++i3 ){
              f32* offset1 = t1->data + t1->offset + i0 * t1->strides[ 0 ] +
                             i1 * t1->strides[ 1 ] + i2 * t1->strides[ 2 ] +
                             i3 * t1->strides[ 3 ];
              f32* offset2 = t2->data + t2->offset + i0 * t2->strides[ 0 ] +
                             i1 * t2->strides[ 1 ] + i2 * t2->strides[ 2 ] +
                             i3 * t2->strides[ 3 ];
              *offset2 = *offset2 + *offset1;
            }

      pop( ts );
      // dbg( "%s", "add" );
      break;
    }
    case SUB: {
      if( ts->size < 2 )
        error( "%s", "Attempt to sub without enough arguments on the stack." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      tensorToHostMemory( ts->stack[ ts->size - 2 ] );
      takeOwnership( ts->stack[ ts->size - 2 ] );
      tensor* t1 = ts->stack[ ts->size - 1 ];
      tensor* t2 = ts->stack[ ts->size - 2 ];
      if( t1->rank != t2->rank || t1->shape[ 0 ] != t2->shape[ 0 ] ||
          t1->shape[ 1 ] != t2->shape[ 1 ] ||
          t1->shape[ 2 ] != t2->shape[ 2 ] || t1->shape[ 3 ] != t2->shape[ 3 ] )
        error( "%s", "Attempt to sub tensors with incompatible shapes." );
      for( u32 i0 = 0; i0 < t1->shape[ 0 ]; ++i0 )
        for( u32 i1 = 0; i1 < t1->shape[ 1 ]; ++i1 )
          for( u32 i2 = 0; i2 < t1->shape[ 2 ]; ++i2 )
            for( u32 i3 = 0; i3 < t1->shape[ 3 ]; ++i3 ){
              f32* offset1 = t1->data + t1->offset + i0 * t1->strides[ 0 ] +
                             i1 * t1->strides[ 1 ] + i2 * t1->strides[ 2 ] +
                             i3 * t1->strides[ 3 ];
              f32* offset2 = t2->data + t2->offset + i0 * t2->strides[ 0 ] +
                             i1 * t2->strides[ 1 ] + i2 * t2->strides[ 2 ] +
                             i3 * t2->strides[ 3 ];
              *offset2 = *offset2 - *offset1;
            }

      pop( ts );
      // dbg( "%s", "sub" );
      break;
    }
    case MUL: {
      if( ts->size < 2 )
        error( "%s", "Attempt to mul without enough arguments on the stack." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      tensorToHostMemory( ts->stack[ ts->size - 2 ] );
      takeOwnership( ts->stack[ ts->size - 2 ] );
      tensor* t1 = ts->stack[ ts->size - 1 ];
      tensor* t2 = ts->stack[ ts->size - 2 ];
      if( t1->rank != t2->rank || t1->shape[ 0 ] != t2->shape[ 0 ] ||
          t1->shape[ 1 ] != t2->shape[ 1 ] ||
          t1->shape[ 2 ] != t2->shape[ 2 ] || t1->shape[ 3 ] != t2->shape[ 3 ] )
        error( "%s", "Attempt to mul tensors with incompatible shapes." );
      for( u32 i0 = 0; i0 < t1->shape[ 0 ]; ++i0 )
        for( u32 i1 = 0; i1 < t1->shape[ 1 ]; ++i1 )
          for( u32 i2 = 0; i2 < t1->shape[ 2 ]; ++i2 )
            for( u32 i3 = 0; i3 < t1->shape[ 3 ]; ++i3 ){
              f32* offset1 = t1->data + t1->offset + i0 * t1->strides[ 0 ] +
                             i1 * t1->strides[ 1 ] + i2 * t1->strides[ 2 ] +
                             i3 * t1->strides[ 3 ];
              f32* offset2 = t2->data + t2->offset + i0 * t2->strides[ 0 ] +
                             i1 * t2->strides[ 1 ] + i2 * t2->strides[ 2 ] +
                             i3 * t2->strides[ 3 ];
              *offset2 = *offset2 * *offset1;
            }

      pop( ts );
      // dbg( "%s", "mul" );
      break;
    }
    case DIV: {
      if( ts->size < 2 )
        error( "%s", "Attempt to div without enough arguments on the stack." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      tensorToHostMemory( ts->stack[ ts->size - 2 ] );
      takeOwnership( ts->stack[ ts->size - 2 ] );
      tensor* t1 = ts->stack[ ts->size - 1 ];
      tensor* t2 = ts->stack[ ts->size - 2 ];
      if( t1->rank != t2->rank || t1->shape[ 0 ] != t2->shape[ 0 ] ||
          t1->shape[ 1 ] != t2->shape[ 1 ] ||
          t1->shape[ 2 ] != t2->shape[ 2 ] || t1->shape[ 3 ] != t2->shape[ 3 ] )
        error( "%s", "Attempt to div tensors with incompatible shapes." );
      for( u32 i0 = 0; i0 < t1->shape[ 0 ]; ++i0 )
        for( u32 i1 = 0; i1 < t1->shape[ 1 ]; ++i1 )
          for( u32 i2 = 0; i2 < t1->shape[ 2 ]; ++i2 )
            for( u32 i3 = 0; i3 < t1->shape[ 3 ]; ++i3 ){
              f32* offset1 = t1->data + t1->offset + i0 * t1->strides[ 0 ] +
                             i1 * t1->strides[ 1 ] + i2 * t1->strides[ 2 ] +
                             i3 * t1->strides[ 3 ];
              f32* offset2 = t2->data + t2->offset + i0 * t2->strides[ 0 ] +
                             i1 * t2->strides[ 1 ] + i2 * t2->strides[ 2 ] +
                             i3 * t2->strides[ 3 ];
              *offset2 = *offset2 / *offset1;
            }

      pop( ts );
      // dbg( "%s", "div" );
      break;
    }
    case TENSOR:
      push( ts, copyTensor( s->tensor ) );
      break;
    case PRINT:
      printStack( ts );
      // dbg( "%s", "print" );
      break;
    case POP:
      pop( ts );
      // dbg( "%s %u", "pop", ts->size );
      break;
    case CALL:
      if( p->numReturns >= p->returnStackSize ){
        p->returnStackSize *= 2;
        u32* t = mem( p->returnStackSize, u32 );
        memcpy( t, p->returns, p->numReturns * sizeof( u32 ) );
        unmem( p->returns );
        p->returns = t;
      }
      p->returns[ p->numReturns++ ] = i;
      i = s->branch - 1;
      // dbg( "%s", "call" );
      break;
    case RETURN:
      if( !p->numReturns )
        error( "%s", "Attempt to return with an empty return stack." );
      i = p->returns[ --p->numReturns ];
      // dbg( "%s", "return" );
      break;
    case COMPUTE:
      if( !ts->size )
        error( "%s",
               "Attempt to run a compute statement with no shape parameter on "
               "the stack." );
      if( ts->stack[ ts->size - 1 ]->rank != 1 )
        error( "%s", "The shape for a compute was not a rank 1 tensor." );
      if( ts->stack[ ts->size - 1 ]->size > 4 )
        error( "%s", "The shape for an initilizer was more than 4 component." );
      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      u32 shape[ 4 ];
      u32 size = ts->stack[ ts->size - 1 ]->size;
      for( u32 i = 0; i < ts->stack[ ts->size - 1 ]->size; ++i )
        shape[ i ] = ts->stack[ ts->size - 1 ]->data[ i ];
      pop( ts );
      // dbg( "%u rc", p->computes[ s->compute ]->retCount );
      tensor** rets =
        newTensorsInitialized( p, ts, size, shape, p->computes[ s->compute ] );
      for( u32 i = 0; i < p->computes[ s->compute ]->retCount; ++i )
        push( ts, rets[ p->computes[ s->compute ]->retCount - i - 1 ] );
      unmem( rets );
      // dbg( "%s", "compute" );
      break;
    case CAT: {
      if( ts->size < 3 )
        error(
          "%s",
          "Attempt to concatenate without enough arguments on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s",
               "Attempt to concatenate with a nonscalar axis parameter." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      u32 axis = *( ts->stack[ ts->size - 1 ]->data +
                    ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      tensorCat( ts, ts->size - 2, ts->size - 1, axis );
      pop( ts );
      // dbg( "%s %u", "cat", axis );
      break;
    }
    case REVERSE: {
      if( !ts->size )
        error( "%s",
               "Attempt to reverse with no axis parameter on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s", "Attempt to reverse a nonscalar axis parameter." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      u32 axis = *( ts->stack[ ts->size - 1 ]->data +
                    ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      tensorReverse( ts, ts->size - 1, axis );
      // dbg( "%s %u", "reverse", axis );
      break;
    }
    case SHAPE: {
      tensor* cur = ts->stack[ ts->size - 1 ];
      f32* newData = mem( cur->rank, f32 );
      for( u32 i = 0; i < cur->rank; ++i )
	newData[ i ] = cur->shape[ i ];
      pop( ts );
      u32 newShape[ 1 ] = { cur->rank };
      push( ts, newTensor( 1, newShape, newData ) );
      // dbg( "%s %u", "shape", axis );
      break;
    }
    case FIRST: {
      if( !ts->size )
        error(
          "%s",
          "Attempt to take the first element with no parameter on the stack." );

      tensorTakeFirst( ts, ts->size - 1 );
      // dbg( "%s", "first" );
      break;
    }
    case LAST: {
      if( !ts->size )
        error(
          "%s",
          "Attempt to take the first element with no parameter on the stack." );

      tensorTakeLast( ts, ts->size - 1 );
      // dbg( "%s", "first" );
      break;
    }
    case DUP:
      if( !ts->size )
        error( "%s", "Attempt to duplicate with no parameter on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s", "Attempt to duplicate with a nonscalar parameter." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      u32 dup = *( ts->stack[ ts->size - 1 ]->data +
                   ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      if( dup > ts->size - 1 )
        error( "%s", "Attempt to duplicate past the end of the stack." );
      push( ts, copyTensor( ts->stack[ ( ts->size - 1 ) - dup ] ) );
      // dbg( "%s %u", "dup", dup );
      break;
    case REPEAT: {
      if( ts->size < 2 )
        error( "%s", "Attempt to repeate without enough parameters on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s", "Attempt to repeat with a nonscalar count." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      u32 count = *( ts->stack[ ts->size - 1 ]->data +
                     ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      tensorRepeat( ts, ts->size - 1, count );
      // dbg( "%s %u", "rep", count );
      break;
    }
    case IF: {
      if( !ts->size )
        error( "%s", "Attempt to if with no parameter on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s", "Attempt to if with a non-scalar parameter." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      f32 cond = *( ts->stack[ ts->size - 1 ]->data +
                    ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      if( cond != 0.0 )
        i = s->branch - 1;
      // dbg( "%s %f", "if", cond );
      break;
    }
    case IFN: {
      if( !ts->size )
        error( "%s", "Attempt to ifn with no parameter on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank )
        error( "%s", "Attempt to ifn with a non-scalar parameter." );

      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      f32 cond = *( ts->stack[ ts->size - 1 ]->data +
                    ts->stack[ ts->size - 1 ]->offset );
      pop( ts );
      if( cond == 0.0 )
        i = s->branch - 1;
      // dbg( "%s %f", "ifn", cond );
      break;
    }
    case TRANSPOSE:
      if( !ts->size )
        error( "%s",
               "Attempt to transpose with no axes parameter on the stack." );
      if( ts->stack[ ts->size - 1 ]->rank != 1 )
        error( "%s",
               "Attempt to transpose with a axes parameter not of rank 1." );

      u32 axis1 = *( ts->stack[ ts->size - 1 ]->data +
                     ts->stack[ ts->size - 1 ]->offset );
      u32 axis2 =
        *( ts->stack[ ts->size - 1 ]->data + ts->stack[ ts->size - 1 ]->offset +
           ts->stack[ ts->size - 1 ]->strides[ 0 ] );
      pop( ts );
      tensorTranspose( ts, ts->size - 1, axis1, axis2 );
      // dbg( "%s %u %u", "transpose", axis1, axis2 );
      break;
    case SLICE:
      if( !ts->size )
        error( "%s", "Attempt to slice with an empty stack." );
      if( ts->stack[ ts->size - 1 ]->rank != 1 )
        error( "%s", "Attempt to slice with a parameter not of rank 1." );

      u32 start = *( ts->stack[ ts->size - 1 ]->data +
                     ts->stack[ ts->size - 1 ]->offset );
      u32 end =
        *( ts->stack[ ts->size - 1 ]->data + ts->stack[ ts->size - 1 ]->offset +
           ts->stack[ ts->size - 1 ]->strides[ 0 ] );
      u32 axis =
        *( ts->stack[ ts->size - 1 ]->data + ts->stack[ ts->size - 1 ]->offset +
           ts->stack[ ts->size - 1 ]->strides[ 0 ] * 2 );
      pop( ts );
      tensorSlice( ts, ts->size - 1, axis, start, end );
      // dbg( "%s %u %u", "transpose", axis1, axis2 );
      break;
    case TOP: {
      f32* ssize = mem( 1, f32 );
      *ssize = ts->size;
      push( ts, newTensor( 0, NULL, ssize ) );
      // dbg( "%s %u %u", "size", axis1, axis2 );
      break;
    }
    case SET:
      if( ( s->var.size <= 4 && ts->stack[ ts->size - 1 ]->rank != 1 ) ||
          ( s->var.size == 16 && ts->stack[ ts->size - 1 ]->rank != 2 ) )
        error( "%s", "Incorrect rank during set statement." );
      if( s->var.size != ts->stack[ ts->size - 1 ]->size ){
        // dbg( "%u %u", s->var.size, ts->stack[ ts->size - 1 ]->size );
        error( "%s", "Incorrect size during set statement." );
      }
      tensorToHostMemory( ts->stack[ ts->size - 1 ] );
      f32* uniform = p->varBlock + p->varOffsets[ s->var.index ];
      if( s->var.size <= 4 )
        for( u32 i = 0; i < s->var.size; ++i )
          uniform[ i ] = *( ts->stack[ ts->size - 1 ]->data +
                            ts->stack[ ts->size - 1 ]->offset +
                            ts->stack[ ts->size - 1 ]->strides[ 0 ] * i );
      else
        for( u32 i = 0; i < 4; ++i )
          for( u32 j = 0; j < 4; ++j )
            uniform[ i * 4 + j ] =
              *( ts->stack[ ts->size - 1 ]->data +
                 ts->stack[ ts->size - 1 ]->offset +
                 ts->stack[ ts->size - 1 ]->strides[ 0 ] * i +
                 ts->stack[ ts->size - 1 ]->strides[ 1 ] * j );
      for( u32 i = 0; i < p->numComputes; ++i ){
        glUseProgram( p->computes[ i ]->program );
        switch( p->varSizes[ s->var.index ] ){
        case 1:
          glUniform1fv( p->computes[ i ]->uniformLocs[ s->var.index ],
                        1,
                        p->varBlock + p->varOffsets[ s->var.index ] );
          break;
        case 2:
          glUniform2fv( p->computes[ i ]->uniformLocs[ s->var.index ],
                        1,
                        p->varBlock + p->varOffsets[ s->var.index ] );
          break;
        case 3:
          glUniform3fv( p->computes[ i ]->uniformLocs[ s->var.index ],
                        1,
                        p->varBlock + p->varOffsets[ s->var.index ] );
          break;
        case 4:
          glUniform4fv( p->computes[ i ]->uniformLocs[ s->var.index ],
                        1,
                        p->varBlock + p->varOffsets[ s->var.index ] );
          break;
        case 16:
          glUniformMatrix4fv( p->computes[ i ]->uniformLocs[ s->var.index ],
                              1,
                              0,
                              p->varBlock + p->varOffsets[ s->var.index ] );
          break;
        default:
          error( "%s", "Logic error in Atlas! Bad variable size." );
        }
      }

      // glBindBuffer( GL_UNIFORM_BUFFER, p->ubo );
      // glBufferSubData( GL_UNIFORM_BUFFER, p->varOffsets[ s->var.index ] *
      // sizeof( f32 ), p->varSizes[ s->var.index ] * sizeof( f32 ), uniform );
      pop( ts );
      // dbg( "%s", "set" );
      break;
    case GET: {
      // dbg( "foo" );
      static const u32 shape1[ 4 ] = { 1 };
      static const u32 shape2[ 4 ] = { 2 };
      static const u32 shape3[ 4 ] = { 3 };
      static const u32 shape4[ 4 ] = { 4 };
      static const u32 shape16[ 2 ] = { 4, 4 };
      const u32* shape;
      u32 rank = 1;
      switch( p->varSizes[ s->var.index ] ){
      case 1:
        shape = shape1;
        break;
      case 2:
        shape = shape2;
        break;
      case 3:
        shape = shape3;
        break;
      case 4:
        shape = shape4;
        break;
      case 16:
        shape = shape16;
        rank = 2;
        break;
      default:
        error( "%s %u.",
               "Logic error in atlas! Bad p->varSizes[ s->var.index ]",
               p->varSizes[ s->var.index ] );
      }
      tensor* t =
        newTensor( rank, shape, p->varBlock + p->varOffsets[ s->var.index ] );
      t->ownsData = false;  // Ensure the tensor does not own the data
      push( ts, t );
      // dbg( "%s", "get" );
      break;
    }
    case QUIT:
      // dbg( "%s", "exit" );
      return false;
    default:
      error( "%s", "Logic error in Atlas!" );
    }
  }
  return true;
}
