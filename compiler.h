#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COMPILER_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COMPILER_H

//--------------------------------------
// 공통 문자열화 매크로
//--------------------------------------
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

//--------------------------------------
// Compiler
//--------------------------------------
#if defined(__clang__)

    #if defined(__apple_build_version__)
        #define COMPILER_NAME "AppleClang"
        #define COMPILER_VERSION \
            STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
    #elif defined(_MSC_VER)
        #define COMPILER_NAME "Clang-CL"
        #define COMPILER_VERSION \
            STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
    #elif defined(__MINGW64__)
        #define COMPILER_NAME "Clang (MinGW-w64)"
        #define COMPILER_VERSION \
            STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
    #elif defined(__MINGW32__)
        #define COMPILER_NAME "Clang (MinGW)"
        #define COMPILER_VERSION \
            STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
    #else
        #define COMPILER_NAME "Clang"
        #define COMPILER_VERSION \
            STR(__clang_major__) "." STR(__clang_minor__) "." STR(__clang_patchlevel__)
    #endif

#elif defined(_MSC_VER)

    #define COMPILER_NAME "MSVC"
    #define COMPILER_VERSION STR(_MSC_FULL_VER)

#elif defined(__MINGW64__)

    #define COMPILER_NAME "MinGW-w64"
    #define COMPILER_VERSION \
        STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)

#elif defined(__MINGW32__)

    #define COMPILER_NAME "MinGW"
    #define COMPILER_VERSION \
        STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)

#elif defined(__GNUC__)

    #define COMPILER_NAME "GCC"
    #define COMPILER_VERSION \
        STR(__GNUC__) "." STR(__GNUC_MINOR__) "." STR(__GNUC_PATCHLEVEL__)

#else

    #define COMPILER_NAME "UnknownCompiler"
    #define COMPILER_VERSION "0"

#endif

//--------------------------------------
// OS
//--------------------------------------
#if defined(_WIN32) || defined(_WIN64)
    #define OS_NAME "Windows"
#elif defined(__linux__)
    #define OS_NAME "Linux"
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_NAME "macOS"
#else
    #define OS_NAME "UnknownOS"
#endif

//--------------------------------------
// Architecture
//--------------------------------------
#if defined(_M_X64) || defined(__x86_64__)
    #define ARCH_NAME "x64"
#elif defined(_M_IX86) || defined(__i386__)
    #define ARCH_NAME "x86"
#elif defined(_M_ARM64) || defined(__aarch64__)
    #define ARCH_NAME "ARM64"
#elif defined(_M_ARM) || defined(__arm__)
    #define ARCH_NAME "ARM"
#else
    #define ARCH_NAME "UnknownArch"
#endif

//--------------------------------------
// Build Type
//--------------------------------------
#if defined(_DEBUG) || !defined(NDEBUG)
    #define BUILD_TYPE "Debug"
#else
    #define BUILD_TYPE "Release"
#endif

//--------------------------------------
// CRT
//--------------------------------------
#if defined(_MSC_VER)
    #if defined(_MT) && defined(_DLL)
        #define CRT_TYPE "MD"
    #elif defined(_MT)
        #define CRT_TYPE "MT"
    #else
        #define CRT_TYPE "SingleThreaded"
    #endif
#else
    #define CRT_TYPE "N/A"
#endif

//--------------------------------------
// 최종 빌드 환경 문자열
//--------------------------------------
#define BUILD_ENV_STRING \
    COMPILER_NAME " " COMPILER_VERSION " | " \
    OS_NAME " | " \
    ARCH_NAME " | " \
    BUILD_TYPE " | CRT:" CRT_TYPE

#endif // RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COMPILER_H