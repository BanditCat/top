////////////////////////////////////////////////////////////////////////////////
// Copyright © 2024 Jon DuBois. Written with the assistance of GPT-4-o1.      //
////////////////////////////////////////////////////////////////////////////////
#ifndef ATLAS_H_INCLUDED
#define ATLAS_H_INCLUDED
#define _CRT_SECURE_NO_WARNINGS


#include "SDL2/SDL.h"
#ifndef __EMSCRIPTEN__
#include "GL/glew.h" // For managing OpenGL extensions
#else
#include <SDL2/SDL_opengles2.h>
#include <GLES3/gl3.h>
#include <emscripten/emscripten.h>
#endif 
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <math.h>


// Function to translate OpenGL error codes to human-readable strings
static inline const char* GetGLErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "No error has been recorded.";
        case GL_INVALID_ENUM:
            return "An unacceptable value is specified for an enumerated argument.";
        case GL_INVALID_VALUE:
            return "A numeric argument is out of range.";
        case GL_INVALID_OPERATION:
            return "The specified operation is not allowed in the current state.";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "The framebuffer object is not complete.";
        case GL_OUT_OF_MEMORY:
            return "There is not enough memory left to execute the command.";
        default:
            return "Unknown OpenGL error.";
    }
}

/* // Macro to check and log OpenGL errors */
/* #define CHECK_GL_ERROR()                                 \ */
/*     do {                                                 \ */
/*         GLenum err;                                      \ */
/*         while ((err = glGetError()) != GL_NO_ERROR) {   \ */
/*             fprintf(stderr, "OpenGL Error: %s (0x%X) at %s:%d\n", \ */
/*                     GetGLErrorString(err), err, __FILE__, __LINE__); \ */
/*             fprintf(stdout, "OpenGL Error: %s (0x%X) at %s:%d\n", \ */
/*                     GetGLErrorString(err), err, __FILE__, __LINE__); \ */
/*             /\* You can choose to exit or handle the error here *\/ \ */
/*         }                                                \ */
/*     } while (0) */
#define CHECK_GL_ERROR() { (void)0; }

#if ULLONG_MAX != 18446744073709551615ULL
#error bad long size
#endif 
#if UINT_MAX != 4294967295
#error bad int size
#endif
#if UCHAR_MAX != 255
#error bad char size
#endif

typedef unsigned long long int u64;
typedef signed long long int s64;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned char u8;
typedef signed char s8;
typedef float f32;
typedef double f64;












/* typedef struct MemAlloc { */
/*     void* ptr; */
/*     size_t size; */
/*     const char* file; */
/*     int line; */
/*     int freed; // Indicates if the memory has been freed */
/*     struct MemAlloc* next; */
/* } MemAlloc; */

/* extern MemAlloc* mem_list; */
/* extern SDL_mutex* mem_list_mutex; // Use SDL mutex */

/* inline void* mem_track(size_t count, size_t size, const char* file, int line) { */
/*     // Initialize the mutex if it's not already initialized */
/*     if (!mem_list_mutex) { */
/*         mem_list_mutex = SDL_CreateMutex(); */
/*         if (!mem_list_mutex) { */
/*             fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError()); */
/*             exit(EXIT_FAILURE); */
/*         } */
/*     } */

/*     void* ptr = calloc(count, size); */
/*     if (!ptr) { */
/*         fprintf(stderr, "Memory allocation failed at %s:%d\n", file, line); */
/*         exit(EXIT_FAILURE); */
/*     } */
/*     MemAlloc* alloc = calloc(1,sizeof(MemAlloc)); */
/*     if (!alloc) { */
/*         fprintf(stderr, "Failed to allocate memory for tracking at %s:%d\n", file, line); */
/*         free(ptr); */
/*         exit(EXIT_FAILURE); */
/*     } */
/*     alloc->ptr = ptr; */
/*     alloc->size = count * size; */
/*     alloc->file = file; */
/*     alloc->line = line; */
/*     alloc->freed = 0; // Set freed flag to 0 */

/*     SDL_LockMutex(mem_list_mutex); */
/*     alloc->next = mem_list; */
/*     mem_list = alloc; */
/*     SDL_UnlockMutex(mem_list_mutex); */

/*     return ptr; */
/* } */

/* inline void unmem_track(void* ptr, const char* file, int line) { */
/*     if (!ptr) { */
/*         fprintf(stderr, "Attempted to free NULL pointer at %s:%d\n", file, line); */
/*         return; */
/*     } */

/*     if (!mem_list_mutex) { */
/*         fprintf(stderr, "Memory tracker mutex not initialized when freeing at %s:%d\n", file, line); */
/*         return; */
/*     } */

/*     SDL_LockMutex(mem_list_mutex); */
/*     MemAlloc* current = mem_list; */
/*     while (current) { */
/*         if (current->ptr == ptr) { */
/*             if (current->freed) { */
/*                 // Double free detected */
/*                 fprintf(stderr, "Double free detected at %s:%d (originally allocated at %s:%d)\n", */
/*                         file, line, current->file, current->line); */
/*             } else { */
/*                 // Mark as freed and free the memory */
/*                 current->freed = 1; */
/*                 free(ptr); */
/*             } */
/*             SDL_UnlockMutex(mem_list_mutex); */
/*             return; */
/*         } */
/*         current = current->next; */
/*     } */
/*     SDL_UnlockMutex(mem_list_mutex); */
/*     fprintf(stderr, "Attempted to free untracked or already freed memory at %s:%d\n", file, line); */
/* } */

/* inline void check_memory_leaks(void) { */
/*     if (mem_list_mutex) { */
/*         SDL_LockMutex(mem_list_mutex); */
/*     } */
/*     MemAlloc* current = mem_list; */
/*     int leaks_found = 0; */
/*     while (current) { */
/*         if (!current->freed) { */
/*             fprintf(stderr, "Memory leak of %zu bytes allocated at %s:%d\n", */
/*                     current->size, current->file, current->line); */
/*             leaks_found = 1; */
/*         } */
/*         MemAlloc* to_free = current; */
/*         current = current->next; */
/*         free(to_free); // Free the tracking struct itself */
/*     } */
/*     mem_list = NULL; */
/*     if (mem_list_mutex) { */
/*         SDL_UnlockMutex(mem_list_mutex); */
/*         SDL_DestroyMutex(mem_list_mutex); */
/*         mem_list_mutex = NULL; */
/*     } */
/*     if (!leaks_found) { */
/*         printf("No memory leaks detected.\n"); */
/*     } */
/* } */
/* // Macros to automatically capture file and line information */
/* #define mem(count, type) mem_track(count, sizeof(type), __FILE__, __LINE__) */
/* #define unmem(ptr) unmem_track(ptr, __FILE__, __LINE__) */










extern u64 memc;
#define mem( size, T ) ( memc++, calloc( ( size ), sizeof( T ) ) )
#define unmem( F ) ( memc--, free( F ) ) 
#define error( msg, ... ) ( fflush( stdout ),\
			    fprintf( stdout, ( msg ), __VA_ARGS__ ), fprintf( stdout, "\n" ),\
			    fflush( stdout ), exit( 1 ) )
#define dbg( msg, ... ) ( fflush( stdout ),\
			  fprintf( stdout, ( msg ), __VA_ARGS__ ), fprintf( stdout, "\n" ),\
			  fflush( stdout ) )

// Global for ease.
extern SDL_Window* window;
extern SDL_GLContext glContext;
extern s32 mouseWheelDelta;
#ifndef __EMSCRIPTEN__
extern SDL_mutex* data_mutex;
#endif

#include "trie.h"
#include "tensor.h"
#include "program.h"

#endif //ATLAS_H_INCLUDED
