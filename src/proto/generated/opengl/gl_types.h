// THIS FILE IS GENERATED. DO NOT EDIT.
// Regenerate with: python -m rvk_codegen.opengl --xml gl.xml --wgl wgl.xml
//                  --profile tools/codegen/profiles/opengl.toml --out ...

#pragma once

#include <cstddef>
#include <cstdint>

// The calling convention every GL entry point is declared
// with, taken the way the Khronos headers take it: from the
// platform where the platform has one, and nothing at all
// where it does not.
#if defined(_WIN32) && !defined(APIENTRY)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef std::int8_t GLbyte;
typedef std::uint8_t GLubyte;
typedef std::int16_t GLshort;
typedef std::uint16_t GLushort;
typedef int GLint;
typedef unsigned int GLuint;
typedef std::int32_t GLclampx;
typedef int GLsizei;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void *GLeglClientBufferEXT;
typedef void *GLeglImageOES;
typedef char GLchar;
typedef char GLcharARB;
#ifdef __APPLE__
typedef void *GLhandleARB;
#else
typedef unsigned int GLhandleARB;
#endif
typedef std::uint16_t GLhalf;
typedef std::uint16_t GLhalfARB;
typedef std::int32_t GLfixed;
typedef std::intptr_t GLintptr;
typedef std::intptr_t GLintptrARB;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLsizeiptrARB;
typedef std::int64_t GLint64;
typedef std::int64_t GLint64EXT;
typedef std::uint64_t GLuint64;
typedef std::uint64_t GLuint64EXT;
typedef struct __GLsync *GLsync;
struct _cl_context;
struct _cl_event;
typedef void ( *GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void ( *GLDEBUGPROCARB)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void ( *GLDEBUGPROCKHR)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);
typedef void ( *GLDEBUGPROCAMD)(GLuint id,GLenum category,GLenum severity,GLsizei length,const GLchar *message,void *userParam);
typedef unsigned short GLhalfNV;
typedef GLintptr GLvdpauSurfaceNV;
typedef void ( *GLVULKANPROCNV)(void);
