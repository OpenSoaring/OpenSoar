#pragma once

#include <jasper/jas_compiler.h>

#define JAS_VERSION "unknown"
#define JAS_ENABLE_32BIT 1
#define JAS_HAVE_FCNTL_H		1
#ifdef _WIN32// TODO(August2111)
#   define  JAS_HAVE_IO_H  1
#   undef JAS_HAVE_UNISTD_H
#   define JAS_HAVE_WINDOWS_H 1
#   undef JAS_HAVE_SYS_TIME_H
#   undef JAS_HAVE_SYS_TYPES_H
#else  // _WIN32// TODO(August2111)
#   undef  JAS_HAVE_IO_H
#   define JAS_HAVE_UNISTD_H 1
#   undef JAS_HAVE_WINDOWS_H
#   define JAS_HAVE_SYS_TIME_H 1
#   define JAS_HAVE_SYS_TYPES_H 1
#endif  // _WIN32// TODO(August2111)
#undef JAS_HAVE_GETTIMEOFDAY
#undef JAS_HAVE_GETRUSAGE

#define JAS_HAVE_SNPRINTF 1

#define JAS_DLLEXPORT
#define JAS_DLLLOCAL

#if !defined(JAS_DEC_DEFAULT_MAX_SAMPLES)
#define JAS_DEC_DEFAULT_MAX_SAMPLES (64 * ((size_t) 1048576))
#endif

#define EXCLUDE_MIF_SUPPORT
#define EXCLUDE_PNM_SUPPORT
#define EXCLUDE_BMP_SUPPORT
#define EXCLUDE_RAS_SUPPORT
#define EXCLUDE_JPG_SUPPORT
#define EXCLUDE_PGX_SUPPORT
#define EXCLUDE_TIFF_SUPPORT
