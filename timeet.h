#ifndef WIN_MAC_LINUX_H_INCLUDED
#define WIN_MAC_LINUX_H_INCLUDED

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<stdint.h>
#include<stddef.h>
#include<stdbool.h>

#define   MAX_BUFFER_SIZE   256

typedef uint8_t utf8_t;
typedef uint16_t utf16_t;

size_t utf16_to_utf8(
    utf16_t const* utf16, size_t utf16_len, 
    utf8_t* utf8,         size_t utf8_len
);

size_t utf8_to_utf16(
    utf8_t const* utf8, size_t utf8_len, 
    utf16_t* utf16,     size_t utf16_len
);

#if defined _WIN32 || defined _WIN64

#include<Windows.h>
#include<TlHelp32.h>

#elif defined __linux__

#include <X11/Xlib.h> 
#include <X11/Xmu/WinUtil.h>
Bool xerror;
Display *display;

#elif defined __APPLE_CC__

int layer;

#endif

int get_window_name(char* );
int get_process_name(char* ); 

#endif

//linux:
// gcc -fPIC -c timeet.c
//gcc -shared timeet.o -o timeet.so -I/usr/X11R6/include:/usr/include/X11 -L/usr/X11R6/lib:/usr/lib/x86_64-linux-gnu -lX11 -lXmu -lXext -lm -Wall
//
//----------------------------------------------------------------------------------------------------------------------
//
//windows:
//gcc -c timeet.c
//gcc -shared timeet.o -o timeet.dll