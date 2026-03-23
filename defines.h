#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DEFINES_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DEFINES_H

#include <stdio.h>

// ░██████╗███████╗██╗░░░░░███████╗░█████╗░████████╗░░░░░░░█████╗░███╗░░██╗██╗░░░██╗
// ██╔════╝██╔════╝██║░░░░░██╔════╝██╔══██╗╚══██╔══╝░░░░░░██╔══██╗████╗░██║╚██╗░██╔╝
// ╚█████╗░█████╗░░██║░░░░░█████╗░░██║░░╚═╝░░░██║░░░░░░░░░███████║██╔██╗██║░╚████╔╝░
// ░╚═══██╗██╔══╝░░██║░░░░░██╔══╝░░██║░░██╗░░░██║░░░░░░░░░██╔══██║██║╚████║░░╚██╔╝░░
// ██████╔╝███████╗███████╗███████╗╚█████╔╝░░░██║░░░█████╗██║░░██║██║░╚███║░░░██║░░░
// ╚═════╝░╚══════╝╚══════╝╚══════╝░╚════╝░░░░╚═╝░░░╚════╝╚═╝░░╚═╝╚═╝░░╚══╝░░░╚═╝░░░
#if defined(__clang__)
#define SELECT_ANY __attribute__((weak))

#elif defined(_MSC_VER)
#define SELECT_ANY __declspec(selectany)

#elif defined(__MINGW32__) || defined(__MINGW64__)
#define SELECT_ANY __attribute__((weak))

#elif defined(__GNUC__)
#define SELECT_ANY __attribute__((weak))

#else
#define SELECT_ANY
#endif

// ░██████╗░█████╗░██╗░░░██╗
// ██╔════╝██╔══██╗╚██╗░██╔╝
// ╚█████╗░███████║░╚████╔╝░
// ░╚═══██╗██╔══██║░░╚██╔╝░░
// ██████╔╝██║░░██║░░░██║░░░
// ╚═════╝░╚═╝░░╚═╝░░░╚═╝░░░
#ifdef _DEBUG

SELECT_ANY void say(const char *format, ...) {
    va_list args;

    va_start(args, format);

    vprintf(format, args);
    puts("");
    va_end(args);
}

#else

#define say(...) ((void)0)

#endif

// ███████╗░█████╗░███╗░░██╗████████╗░██████╗
// ██╔════╝██╔══██╗████╗░██║╚══██╔══╝██╔════╝
// █████╗░░██║░░██║██╔██╗██║░░░██║░░░╚█████╗░
// ██╔══╝░░██║░░██║██║╚████║░░░██║░░░░╚═══██╗
// ██║░░░░░╚█████╔╝██║░╚███║░░░██║░░░██████╔╝
// ╚═╝░░░░░░╚════╝░╚═╝░░╚══╝░░░╚═╝░░░╚═════╝░
#define FONT_DEFAULT_FAMILY "Roboto"
#define FONT_MONOSPACE_FAMILY "Cascadia Code NF"
#define FONT_SUBTITLE_FAMILY "Noto Sans KR"


#endif
