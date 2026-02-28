#ifndef __CONTEXT_MENU_H__
#define __CONTEXT_MENU_H__

#include <Archimedes.h>

#define CTX_MENU_W      100.0f
#define CTX_MENU_ROW_H   24.0f
#define CTX_MENU_PAD      4.0f
#define CTX_MENU_TEXT_S    1.1f

/* Draw a context menu at (x, y) with the given labels.
   Returns the total height of the menu. */
float DrawContextMenu( float x, float y,
                       const char** labels, int count,
                       int cursor );

#endif
