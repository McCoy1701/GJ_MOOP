#include <Archimedes.h>

#include "npc_relocate.h"
#include "dialogue.h"

/* ---- Phase enum ---- */
enum {
  RL_NONE = 0,
  RL_FADE_OUT,
  RL_HOLD,
  RL_FADE_IN,
};

/* ---- Timing ---- */
#define RL_FADE_DUR  0.5f
#define RL_HOLD_DUR  0.3f

/* ---- State ---- */
static int   rl_phase = RL_NONE;
static float rl_timer = 0.0f;
static int   rl_alpha = 0;

/* NPC to relocate */
static int   rl_npc_type  = -1;
static int   rl_dest_row  = 0;
static int   rl_dest_col  = 0;

/* NPC list (set once from game_scene) */
static NPC_t* rl_npcs     = NULL;
static int*   rl_num_npcs = NULL;

/* ------------------------------------------------------------------ */

void NPCRelocateInit( NPC_t* npcs, int* count )
{
  rl_npcs     = npcs;
  rl_num_npcs = count;
  rl_phase    = RL_NONE;
}

void NPCRelocate( int npc_type_idx, int new_row, int new_col, int fade )
{
  if ( rl_phase != RL_NONE ) return;   /* one at a time */

  rl_npc_type = npc_type_idx;
  rl_dest_row = new_row;
  rl_dest_col = new_col;

  if ( !fade )
  {
    /* Instant teleport, no fade */
    for ( int i = 0; i < *rl_num_npcs; i++ )
    {
      if ( rl_npcs[i].alive && rl_npcs[i].type_idx == rl_npc_type )
      {
        rl_npcs[i].row     = new_row;
        rl_npcs[i].col     = new_col;
        rl_npcs[i].world_x = new_col * 16 + 8.0f;
        rl_npcs[i].world_y = new_row * 16 + 8.0f;
        break;
      }
    }
    return;
  }

  rl_phase = RL_FADE_OUT;
  rl_timer = 0.0f;
  rl_alpha = 0;
}

int NPCRelocateUpdate( float dt )
{
  if ( rl_phase == RL_NONE ) return 0;

  /* ESC skips */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;

    /* Apply position immediately */
    for ( int i = 0; i < *rl_num_npcs; i++ )
    {
      if ( rl_npcs[i].alive && rl_npcs[i].type_idx == rl_npc_type )
      {
        rl_npcs[i].row     = rl_dest_row;
        rl_npcs[i].col     = rl_dest_col;
        rl_npcs[i].world_x = rl_dest_col * 16 + 8.0f;
        rl_npcs[i].world_y = rl_dest_row * 16 + 8.0f;
        break;
      }
    }

    rl_phase = RL_NONE;
    rl_alpha = 0;
    return 0;
  }

  rl_timer += dt;

  switch ( rl_phase )
  {
    case RL_FADE_OUT:
    {
      float t = rl_timer / RL_FADE_DUR;
      if ( t > 1.0f ) t = 1.0f;
      rl_alpha = (int)( 255 * t );

      if ( rl_timer >= RL_FADE_DUR )
      {
        rl_alpha = 255;
        rl_timer = 0.0f;
        rl_phase = RL_HOLD;

        /* Move the NPC while screen is black */
        for ( int i = 0; i < *rl_num_npcs; i++ )
        {
          if ( rl_npcs[i].alive && rl_npcs[i].type_idx == rl_npc_type )
          {
            rl_npcs[i].row     = rl_dest_row;
            rl_npcs[i].col     = rl_dest_col;
            rl_npcs[i].world_x = rl_dest_col * 16 + 8.0f;
            rl_npcs[i].world_y = rl_dest_row * 16 + 8.0f;
            break;
          }
        }
      }
      break;
    }

    case RL_HOLD:
      if ( rl_timer >= RL_HOLD_DUR )
      {
        rl_timer = 0.0f;
        rl_phase = RL_FADE_IN;
      }
      break;

    case RL_FADE_IN:
    {
      float t = rl_timer / RL_FADE_DUR;
      if ( t > 1.0f ) t = 1.0f;
      rl_alpha = 255 - (int)( 255 * t );

      if ( rl_timer >= RL_FADE_DUR )
      {
        rl_alpha = 0;
        rl_phase = RL_NONE;
        return 0;
      }
      break;
    }
  }

  return 1;
}

void NPCRelocateDraw( void )
{
  if ( rl_phase == RL_NONE || rl_alpha <= 0 ) return;

  a_DrawFilledRect(
    (aRectf_t){ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT },
    (aColor_t){ 0, 0, 0, rl_alpha }
  );
}
