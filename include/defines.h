#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <Archimedes.h>
#include "player.h"

#define GFX_IMAGE  0
#define GFX_ASCII  1

#define WORLD_WIDTH   2000.0f
#define WORLD_HEIGHT  2000.0f

typedef struct
{
  int gfx_mode;
} GameSettings_t;

extern Player_t player;
extern GameSettings_t settings;

#endif
