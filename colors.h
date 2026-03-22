#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COLORS_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COLORS_H
#include <GL/gl.h>
#include "defines.h"

typedef struct Color3 {
    int r, g, b;
} Color3;

static void glColor4f_int(int r, int g, int b, float a) {
    glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a);
}

#define GL_COLOR_4F(R,G,B,A) glColor4f_int(R,G,B,A);

// static void glColor4d_int(int r, int g, int b, double a) {
//     glColor4d(r / 255.0, g / 255.0, b / 255.0, a);
// }


#endif //RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_COLORS_H
