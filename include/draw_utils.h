#ifndef __DRAW_UTILS_H__
#define __DRAW_UTILS_H__

#include <Archimedes.h>

void DrawImageOrGlyph( aImage_t* img, const char* glyph, aColor_t color,
                       float x, float y, float size );

void DrawButton( float x, float y, float w, float h,
                 const char* label, float text_scale, int hovered,
                 aColor_t bg, aColor_t bg_hover,
                 aColor_t fg, aColor_t fg_hover );

int PointInRect( int mx, int my, float rx, float ry, float rw, float rh );

#endif
