cmake_minimum_required(VERSION 4.1)
project(PXVY C)

set(CMAKE_C_STANDARD 11)

# Execute: perl update_version.pl
# Execute: perl build_time.pl

set(EXE_NAME PXVY)

add_executable(${EXE_NAME} WIN32 main.c app.rc "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/src/sqlite3.c")


# Win32 + OpenGL
target_link_libraries(${EXE_NAME} PRIVATE opengl32 gdi32 comdlg32 user32 shell32 dwmapi gdiplus ole32 oleaut32 msimg32)

if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(COMPILE_OPTIONS -Ofast -funroll-loops -floop-nest-optimize
            -fstrict-overflow -fomit-frame-pointer
            -fipa-cp-clone)
elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-deprecated-declarations)
    add_definitions(-DDO_FORCE_INLINE)
    set(COMPILE_OPTIONS -O3 -flto -funroll-loops -ffast-math
            -fomit-frame-pointer -fvectorize -fslp-vectorize -fno-finite-math-only
            -fstrict-aliasing -finline-functions)
elseif (MSVC)
    message(FATAL_ERROR "MSVC is not supported. Please use MinGW, clang.")
    # set(COMPILE_OPTIONS /O2 /W1 /GL /Ot /fp:fast /arch:AVX2 /GR- /std:c17)
    # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
endif ()


# === === === === 3rdparty === === === ===
set(_3RDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty")
set(_3RDPARTY_INCLUDE_DIR "${_3RDPARTY_DIR}/include")
set(_3RDPARTY_LIB_DIR "${_3RDPARTY_DIR}/lib")
set(_3RDPARTY_BIN_DIR "${_3RDPARTY_DIR}/bin")
set(_3RDPARTY_RES_DIR "${_3RDPARTY_DIR}/res")

if (EXISTS "${_3RDPARTY_INCLUDE_DIR}")
    target_include_directories(${EXE_NAME} PRIVATE "${_3RDPARTY_INCLUDE_DIR}")
endif ()

if (EXISTS "${_3RDPARTY_LIB_DIR}")
    target_link_directories(${EXE_NAME} PRIVATE "${_3RDPARTY_LIB_DIR}")
    file(GLOB _3rdparty_libs
            "${_3RDPARTY_LIB_DIR}/*.a" "${_3RDPARTY_LIB_DIR}/*.so" "${_3RDPARTY_LIB_DIR}/*.dylib" "${_3RDPARTY_LIB_DIR}/*.lib")
    if (_3rdparty_libs)
        target_link_libraries(${EXE_NAME} PRIVATE ${_3rdparty_libs})
    endif ()
endif ()

if (EXISTS "${_3RDPARTY_BIN_DIR}")
    file(GLOB _3rdparty_bins "${_3RDPARTY_BIN_DIR}/*")
    if (_3rdparty_bins)
        add_custom_command(TARGET ${EXE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_3rdparty_bins}
                $<TARGET_FILE_DIR:${EXE_NAME}>
        )
    endif ()
endif ()

if (EXISTS "${_3RDPARTY_RES_DIR}")
    file(GLOB _3rdparty_res "${_3RDPARTY_RES_DIR}/*")
    if (_3rdparty_res)
        add_custom_command(TARGET ${EXE_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${_3rdparty_res}
                $<TARGET_FILE_DIR:${EXE_NAME}>
        )
    endif ()
endif ()