/*
 * editor.h:
 *
 * Copyright (c) 2026 Jacob Kellum <jkellum819@gmail.com>
 *                    Mathew Storm <smattymat@gmail.com>
 ************************************************************************
 */

#ifndef __EDITOR_H__
#define __EDITOR_H__

#include <Archimedes.h>

#include "ed_defines.h"
#include "ed_structs.h"

extern GlyphArray_t* game_glyphs;
extern aColor_t master_colors[MAX_COLOR_GROUPS][48];

void EditorInit( void );

#endif

