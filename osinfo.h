#ifndef _OS_INFO_H
#define _OS_INFO_H

#ifdef WIN32
#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
#define CPUSTRING   "x64"
#define OS_STRING "Win64"
#elif defined (_M_IX86)
#define CPUSTRING   "x86"
#define OS_STRING "Win32"
#elif defined _M_ALPHA
#define CPUSTRING   "AXP"
#define OS_STRING "Win32"
#endif // _M_IX86

#else   // !WIN32

#ifdef __linux__
#define OS_STRING "Linux"
#elif __APPLE__
#define OS_STRING "Mac OSX"
#elif  __sun__
#define OS_STRING "Solaris"
#elif __FreeBSD__
#define OS_STRING "FreeBSD"
#else
#define OS_STRING "UNKNOWN"
#endif // LINUX


#if defined (__x86_64__)
#define CPUSTRING "x64"
#elif  defined(__i386__)
#define CPUSTRING "x86"
#else
#define CPUSTRING "NON-x86"
#endif // __i386__

#endif // !WIN32

#endif // _OS_INFO_H
