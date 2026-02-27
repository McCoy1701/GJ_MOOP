#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "console.h"
#include "game_events.h"
#include "game_scene.h"
#include "main_menu.h"

static void gs_Logic( float );
static void gs_Draw( float );

/* Top bar layout */
#define TB_PAD_X        12.0f
#define TB_PAD_Y         8.0f
#define TB_NAME_SCALE    1.6f
#define TB_STAT_SCALE    1.2f
#define TB_STAT_GAP     12.0f   /* gap between each stat token */
#define TB_SECTION_GAP  32.0f   /* gap between name and stats */
#define TB_ESC_SCALE     1.0f

/* Viewport zoom limits (world units for height) */
#define ZOOM_MIN_H       50.0f
#define ZOOM_MAX_H       300.0f

/* Panel colors */
#define PANEL_BG  (aColor_t){ 0, 0, 0, 200 }
#define PANEL_FG  (aColor_t){ 255, 255, 255, 255 }
#define GOLD      (aColor_t){ 218, 175, 32, 255 }
#define CELL_BG   (aColor_t){ 40, 40, 40, 255 }
#define CELL_FG   (aColor_t){ 80, 80, 80, 255 }
#define GREY      (aColor_t){ 160, 160, 160, 255 }

/* Inventory grid layout */
#define INV_TITLE_H   30.0f
#define INV_PAD        4.0f

/* Equipment panel layout */
#define EQ_TITLE_H    30.0f
#define EQ_ROW_H      41.0f
#define EQ_PAD         6.0f
#define EQ_ICON_SIZE  24.0f
#define EQ_TEXT_SCALE  1.0f

/* Equipment tooltip modal */
#define EQ_MODAL_W       220.0f
#define EQ_MODAL_H       130.0f
#define EQ_MODAL_PAD_X   10.0f
#define EQ_MODAL_PAD_Y    8.0f
#define EQ_MODAL_NAME_S   1.4f
#define EQ_MODAL_TEXT_S   1.1f
#define EQ_MODAL_DESC_S   1.0f
#define EQ_MODAL_LINE_SM  18.0f
#define EQ_MODAL_LINE_MD  22.0f
#define EQ_MODAL_LINE_LG  24.0f

/* Equipment action menu */
#define EQ_ACTION_COUNT   2
#define EQ_ACTION_W     100.0f
#define EQ_ACTION_ROW_H  24.0f
#define EQ_ACTION_PAD     4.0f

static const char* eq_slot_labels[EQUIP_SLOTS] = { "WPN", "ARM", "TRK", "TRK" };
static const char* eq_action_labels[EQ_ACTION_COUNT] = { "Unequip", "Look" };

static int eq_action_open   = 0;
static int eq_action_cursor = 0;

static int inv_action_open   = 0;
static int inv_action_cursor = 0;

static int ui_focus = 0;  /* 0 = game viewport, 1 = inventory panels */

static int prev_mx = -1;
static int prev_my = -1;
static int mouse_moved = 0;


static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;

static Console_t console;

void GameSceneInit( void )
{
  app.delegate.logic = gs_Logic;
  app.delegate.draw  = gs_Draw;

  app.options.scale_factor = 1;

  a_WidgetsInit( "resources/widgets/game_scene.auf" );
  app.active_widget = a_GetWidget( "inv_panel" );

  /* Center viewport on player — w,h must match panel aspect ratio */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float aspect = vp->rect.w / vp->rect.h;
    float vh = 200.0f;
    float vw = vh * aspect;
    app.g_viewport = (aRectf_t){
      player.world_x, player.world_y,
      vw, vh
    };
  }

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  ConsoleInit( &console );
  GameEventsInit( &console );
  ConsolePush( &console, "Welcome, adventurer.", white );
}

static void gs_Logic( float dt )
{
  a_DoInput();

  /* Track whether the mouse actually moved this frame */
  mouse_moved = ( app.mouse.x != prev_mx || app.mouse.y != prev_my );
  prev_mx = app.mouse.x;
  prev_my = app.mouse.y;

  /* Close action menu on Escape, or go to main menu if no menu open */
  if ( app.keyboard[SDL_SCANCODE_ESCAPE] == 1 )
  {
    app.keyboard[SDL_SCANCODE_ESCAPE] = 0;
    if ( eq_action_open || inv_action_open )
    {
      eq_action_open = 0;
      inv_action_open = 0;
    }
    else
    {
      a_WidgetCacheFree();
      MainMenuInit();
      return;
    }
  }

  if ( app.keyboard[A_R] == 1 )
  {
    app.keyboard[A_R] = 0;
    a_WidgetsInit( "resources/widgets/game_scene.auf" );
  }

  /* --- Action menu input (blocks everything else while open) --- */
  if ( eq_action_open )
  {
    int eq_idx = player.equipment[player.equip_cursor];
    EquipmentInfo_t* e = ( eq_idx >= 0 && eq_idx < g_num_equipment ) ? &g_equipment[eq_idx] : NULL;

    /* W/S/arrows — navigate action menu */
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      eq_action_cursor--;
      if ( eq_action_cursor < 0 ) eq_action_cursor = EQ_ACTION_COUNT - 1;
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      eq_action_cursor++;
      if ( eq_action_cursor >= EQ_ACTION_COUNT ) eq_action_cursor = 0;
      a_AudioPlaySound( &sfx_move, NULL );
    }

    /* Mouse hover on action menu rows */
    if ( mouse_moved )
    {
      int mx = app.mouse.x;
      int my = app.mouse.y;
      aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
      float modal_x = kp->rect.x - EQ_MODAL_W - 8;
      float modal_y = kp->rect.y + EQ_TITLE_H + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
      if ( modal_y + EQ_MODAL_H > kp->rect.y + kp->rect.h ) modal_y = kp->rect.y + kp->rect.h - EQ_MODAL_H;
      float ax = modal_x - EQ_ACTION_W - 4;
      if ( ax < 0 ) ax = kp->rect.x + kp->rect.w + 4;
      float ay = modal_y;
      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = ay + i * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );
        if ( PointInRect( mx, my, ax, ry, EQ_ACTION_W, EQ_ACTION_ROW_H ) )
        {
          if ( i != eq_action_cursor )
            a_AudioPlaySound( &sfx_move, NULL );
          eq_action_cursor = i;
          break;
        }
      }
    }

    /* Space/Enter or left-click — execute action */
    int exec = 0;
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      exec = 1;
    }
    if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    {
      app.mouse.pressed = 0;  /* consume click */

      int mx = app.mouse.x;
      int my = app.mouse.y;
      aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );

      /* Check if click is on the action menu buttons */
      float modal_x = kp->rect.x - EQ_MODAL_W - 8;
      float modal_y = kp->rect.y + EQ_TITLE_H + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
      if ( modal_y + EQ_MODAL_H > kp->rect.y + kp->rect.h ) modal_y = kp->rect.y + kp->rect.h - EQ_MODAL_H;
      float action_h = EQ_ACTION_COUNT * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );
      float ax = modal_x - EQ_ACTION_W - 4;
      if ( ax < 0 ) ax = kp->rect.x + kp->rect.w + 4;
      float ay = modal_y;

      if ( PointInRect( mx, my, ax, ay, EQ_ACTION_W, action_h ) )
      {
        exec = 1;
      }
      else
      {
        /* Check if click is on the same equipment row — double-click = execute first action */
        float ey = kp->rect.y + EQ_TITLE_H;
        float row_y = ey + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
        if ( PointInRect( mx, my, kp->rect.x + EQ_PAD, row_y, kp->rect.w - EQ_PAD * 2, EQ_ROW_H ) )
        {
          eq_action_cursor = 0;
          exec = 1;
        }
        else
          eq_action_open = 0;
      }
    }

    if ( exec && e )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      if ( eq_action_cursor == 0 ) /* Unequip */
      {
        int inv_slot = InventoryAdd( INV_EQUIPMENT, eq_idx );
        if ( inv_slot >= 0 )
        {
          GameEvent( EVT_UNEQUIP, eq_idx );
          player.equipment[player.equip_cursor] = -1;
        }
      }
      else if ( eq_action_cursor == 1 ) /* Look */
      {
        GameEvent( EVT_LOOK_EQUIPMENT, eq_idx );
      }
      eq_action_open = 0;
    }

    /* Consume remaining input so nothing else processes */
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_TAB] = 0;
    goto input_done;
  }

  /* --- Inventory action menu input (blocks everything else while open) --- */
  if ( inv_action_open )
  {
    InvSlot_t* slot = &player.inventory[player.inv_cursor];

    /* W/S/arrows — navigate action menu */
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      inv_action_cursor--;
      if ( inv_action_cursor < 0 ) inv_action_cursor = EQ_ACTION_COUNT - 1;
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      inv_action_cursor++;
      if ( inv_action_cursor >= EQ_ACTION_COUNT ) inv_action_cursor = 0;
      a_AudioPlaySound( &sfx_move, NULL );
    }

    /* Mouse hover on action menu rows */
    if ( mouse_moved )
    {
      int mx = app.mouse.x;
      int my = app.mouse.y;
      aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
      aRectf_t r = ip->rect;
      float grid_y = r.y + INV_TITLE_H;
      float grid_w = r.w - INV_PAD * 2;
      float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
      float cell_w = grid_w / INV_COLS;
      float cell_h = grid_h / INV_ROWS;
      float cell = cell_w < cell_h ? cell_w : cell_h;
      float total_gh = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
      float oy = ( grid_h - total_gh ) / 2.0f;
      int row = player.inv_cursor / INV_COLS;
      float cell_y = grid_y + oy + row * ( cell + 2 );

      /* Modal position with bottom clamp (must match draw code) */
      float modal_y = cell_y;
      if ( modal_y + EQ_MODAL_H > ip->rect.y + ip->rect.h )
        modal_y = ip->rect.y + ip->rect.h - EQ_MODAL_H;

      /* Action menu position: left of tooltip, or right of panel if off-screen */
      float modal_x = ip->rect.x - EQ_MODAL_W - 8;
      float amx = modal_x - EQ_ACTION_W - 4;
      if ( amx < 0 ) amx = ip->rect.x + ip->rect.w + 4;
      float amy = modal_y;

      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = amy + i * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );
        if ( PointInRect( mx, my, amx, ry, EQ_ACTION_W, EQ_ACTION_ROW_H ) )
        {
          if ( i != inv_action_cursor )
            a_AudioPlaySound( &sfx_move, NULL );
          inv_action_cursor = i;
          break;
        }
      }
    }

    /* Space/Enter or left-click — execute action */
    int exec = 0;
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      exec = 1;
    }
    if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT )
    {
      app.mouse.pressed = 0;

      int mx = app.mouse.x;
      int my = app.mouse.y;
      aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
      aRectf_t r = ip->rect;
      float grid_y = r.y + INV_TITLE_H;
      float grid_w = r.w - INV_PAD * 2;
      float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
      float cell_w = grid_w / INV_COLS;
      float cell_h = grid_h / INV_ROWS;
      float cell = cell_w < cell_h ? cell_w : cell_h;
      float total_gh = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
      float oy = ( grid_h - total_gh ) / 2.0f;
      int row = player.inv_cursor / INV_COLS;
      float cell_y = grid_y + oy + row * ( cell + 2 );

      /* Modal position with bottom clamp (must match draw code) */
      float modal_y = cell_y;
      if ( modal_y + EQ_MODAL_H > ip->rect.y + ip->rect.h )
        modal_y = ip->rect.y + ip->rect.h - EQ_MODAL_H;

      float modal_x = ip->rect.x - EQ_MODAL_W - 8;
      float amx = modal_x - EQ_ACTION_W - 4;
      if ( amx < 0 ) amx = ip->rect.x + ip->rect.w + 4;
      float amy = modal_y;
      float action_h = EQ_ACTION_COUNT * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );

      if ( PointInRect( mx, my, amx, amy, EQ_ACTION_W, action_h ) )
      {
        exec = 1;
      }
      else
      {
        /* Check if click is on the same inventory cell — double-click = execute first action */
        float total_gw = cell * INV_COLS + 4 * ( INV_COLS - 1 );
        float ox = ( grid_w - total_gw ) / 2.0f;
        int cur_col = player.inv_cursor % INV_COLS;
        float cx = r.x + INV_PAD + ox + cur_col * ( cell + 4 );
        float cy = grid_y + oy + row * ( cell + 2 );
        if ( PointInRect( mx, my, cx, cy, cell - 1, cell - 1 ) )
        {
          inv_action_cursor = 0;
          exec = 1;
        }
        else
          inv_action_open = 0;
      }
    }

    if ( exec && slot->type != INV_EMPTY )
    {
      a_AudioPlaySound( &sfx_click, NULL );
      if ( inv_action_cursor == 0 ) /* Use / Equip */
      {
        if ( slot->type == INV_EQUIPMENT )
        {
          int eq_slot = EquipSlotForKind( g_equipment[slot->index].slot );
          if ( eq_slot >= 0 )
          {
            if ( player.equipment[eq_slot] >= 0 )
            {
              int old = player.equipment[eq_slot];
              GameEventSwap( slot->index, old );
              player.equipment[eq_slot] = slot->index;
              slot->type = INV_EQUIPMENT;
              slot->index = old;
            }
            else
            {
              GameEvent( EVT_EQUIP, slot->index );
              player.equipment[eq_slot] = slot->index;
              slot->type = INV_EMPTY;
              slot->index = 0;
            }
          }
        }
        else if ( slot->type == INV_CONSUMABLE )
        {
          GameEvent( EVT_USE_CONSUMABLE, slot->index );
          slot->type = INV_EMPTY;
          slot->index = 0;
        }
      }
      else if ( inv_action_cursor == 1 ) /* Look */
      {
        if ( slot->type == INV_EQUIPMENT )
          GameEvent( EVT_LOOK_EQUIPMENT, slot->index );
        else if ( slot->type == INV_CONSUMABLE )
          GameEvent( EVT_LOOK_CONSUMABLE, slot->index );
      }
      inv_action_open = 0;
    }

    /* Consume remaining input */
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
    app.keyboard[SDL_SCANCODE_SPACE] = 0;
    app.keyboard[SDL_SCANCODE_RETURN] = 0;
    app.keyboard[SDL_SCANCODE_TAB] = 0;
    goto input_done;
  }

  /* Tab — toggle game viewport / inventory panels */
  if ( app.keyboard[SDL_SCANCODE_TAB] == 1 )
  {
    app.keyboard[SDL_SCANCODE_TAB] = 0;
    ui_focus = !ui_focus;
    if ( ui_focus == 1 ) player.inv_focused = 1;  /* default to inventory grid */
    a_AudioPlaySound( &sfx_move, NULL );
  }

  /* Keyboard nav only when inventory is focused */
  if ( ui_focus == 1 && player.inv_focused )
  {
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      if ( player.inv_cursor < INV_COLS )
      {
        /* top row — jump to bottom equipment slot */
        player.inv_focused = 0;
        player.equip_cursor = EQUIP_SLOTS - 1;
      }
      else
      {
        player.inv_cursor -= INV_COLS;
      }
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      if ( player.inv_cursor + INV_COLS >= MAX_INVENTORY )
      {
        /* bottom row — jump to equipment */
        player.inv_focused = 0;
        player.equip_cursor = 0;
      }
      else
      {
        player.inv_cursor += INV_COLS;
      }
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_A] == 1 || app.keyboard[SDL_SCANCODE_LEFT] == 1 )
    {
      app.keyboard[SDL_SCANCODE_A] = 0;
      app.keyboard[SDL_SCANCODE_LEFT] = 0;
      if ( player.inv_cursor % INV_COLS > 0 )
        player.inv_cursor--;
      else
        player.inv_cursor += INV_COLS - 1;
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_D] == 1 || app.keyboard[SDL_SCANCODE_RIGHT] == 1 )
    {
      app.keyboard[SDL_SCANCODE_D] = 0;
      app.keyboard[SDL_SCANCODE_RIGHT] = 0;
      if ( player.inv_cursor % INV_COLS < INV_COLS - 1 )
        player.inv_cursor++;
      else
        player.inv_cursor -= INV_COLS - 1;
      a_AudioPlaySound( &sfx_move, NULL );
    }

    /* Space/Enter — open action menu on inventory item */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      InvSlot_t* s = &player.inventory[player.inv_cursor];
      if ( s->type != INV_EMPTY )
      {
        a_AudioPlaySound( &sfx_click, NULL );
        inv_action_open = 1;
        inv_action_cursor = 0;
      }
    }
  }
  else if ( ui_focus == 1 ) /* Equipment navigation (WASD + arrows) */
  {
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      if ( player.equip_cursor == 0 )
      {
        /* top of equipment — jump to inventory bottom row */
        player.inv_focused = 1;
        player.inv_cursor = MAX_INVENTORY - INV_COLS;
      }
      else
      {
        player.equip_cursor--;
      }
      a_AudioPlaySound( &sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      if ( player.equip_cursor >= EQUIP_SLOTS - 1 )
      {
        /* bottom of equipment — loop to inventory top row */
        player.inv_focused = 1;
        player.inv_cursor = 0;
      }
      else
      {
        player.equip_cursor++;
      }
      a_AudioPlaySound( &sfx_move, NULL );
    }

    /* Space/Enter — open action menu on equipped item */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      if ( player.equipment[player.equip_cursor] >= 0 )
      {
        a_AudioPlaySound( &sfx_click, NULL );
        eq_action_open = 1;
        eq_action_cursor = 0;
      }
    }
  }

  /* Mouse hover + click — equipment rows */
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    aRectf_t r = kp->rect;
    float ey = r.y + EQ_TITLE_H;

    for ( int i = 0; i < EQUIP_SLOTS; i++ )
    {
      float row_y = ey + i * ( EQ_ROW_H + EQ_PAD );
      if ( PointInRect( mx, my, r.x + EQ_PAD, row_y, r.w - EQ_PAD * 2, EQ_ROW_H ) )
      {
        if ( mouse_moved )
        {
          if ( i != player.equip_cursor || player.inv_focused || ui_focus == 0 )
            a_AudioPlaySound( &sfx_move, NULL );
          player.equip_cursor = i;
          player.inv_focused = 0;
          ui_focus = 1;
        }

        /* Click opens action menu */
        if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT && player.equipment[i] >= 0 )
        {
          app.mouse.pressed = 0;
          a_AudioPlaySound( &sfx_click, NULL );
          eq_action_open = 1;
          eq_action_cursor = 0;
        }
        break;
      }
    }
  }

  /* Mouse hover — inventory cells */
  {
    int mx = app.mouse.x;
    int my = app.mouse.y;
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aRectf_t r = ip->rect;

    float grid_y = r.y + INV_TITLE_H;
    float grid_w = r.w - INV_PAD * 2;
    float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
    float cell_w = grid_w / INV_COLS;
    float cell_h = grid_h / INV_ROWS;
    float cell = cell_w < cell_h ? cell_w : cell_h;
    float total_gw = cell * INV_COLS + 4 * ( INV_COLS - 1 );
    float total_gh = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
    float ox = ( grid_w - total_gw ) / 2.0f;
    float oy = ( grid_h - total_gh ) / 2.0f;

    for ( int row = 0; row < INV_ROWS; row++ )
    {
      for ( int col = 0; col < INV_COLS; col++ )
      {
        float cx = r.x + INV_PAD + ox + col * ( cell + 4 );
        float cy = grid_y + oy + row * ( cell + 2 );
        if ( PointInRect( mx, my, cx, cy, cell - 1, cell - 1 ) )
        {
          int idx = row * INV_COLS + col;
          if ( mouse_moved )
          {
            if ( idx != player.inv_cursor || !player.inv_focused || ui_focus == 0 )
              a_AudioPlaySound( &sfx_move, NULL );
            player.inv_cursor = idx;
            player.inv_focused = 1;
            ui_focus = 1;
          }

          /* Click opens action menu */
          if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT &&
               player.inventory[idx].type != INV_EMPTY )
          {
            app.mouse.pressed = 0;
            a_AudioPlaySound( &sfx_click, NULL );
            inv_action_open = 1;
            inv_action_cursor = 0;
          }
          goto hover_done;
        }
      }
    }
    hover_done:;
  }

  /* Click outside inventory/equipment panels — switch focus back to game */
  if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT && ui_focus == 1 )
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    int on_inv = PointInRect( app.mouse.x, app.mouse.y, ip->rect.x, ip->rect.y, ip->rect.w, ip->rect.h );
    int on_eq  = PointInRect( app.mouse.x, app.mouse.y, kp->rect.x, kp->rect.y, kp->rect.w, kp->rect.h );
    if ( !on_inv && !on_eq )
    {
      ui_focus = 0;
      eq_action_open = 0;
      inv_action_open = 0;
    }
  }

  input_done:

  /* When inventory is focused, consume WASD/arrows so viewport only gets zoom */
  if ( ui_focus == 1 )
  {
    app.keyboard[SDL_SCANCODE_W] = 0;
    app.keyboard[SDL_SCANCODE_S] = 0;
    app.keyboard[SDL_SCANCODE_A] = 0;
    app.keyboard[SDL_SCANCODE_D] = 0;
    app.keyboard[SDL_SCANCODE_UP] = 0;
    app.keyboard[SDL_SCANCODE_DOWN] = 0;
    app.keyboard[SDL_SCANCODE_LEFT] = 0;
    app.keyboard[SDL_SCANCODE_RIGHT] = 0;
  }

  /* Mouse wheel over console — scroll instead of zooming viewport */
  if ( app.mouse.wheel != 0 )
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    if ( PointInRect( app.mouse.x, app.mouse.y,
                      cp->rect.x, cp->rect.y, cp->rect.w, cp->rect.h ) )
    {
      ConsoleScroll( &console, app.mouse.wheel );
      app.mouse.wheel = 0;
    }
  }

  a_ViewportInput( &app.g_viewport, WORLD_WIDTH, WORLD_HEIGHT );

  /* Clamp zoom and fix aspect ratio */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    float aspect = vp->rect.w / vp->rect.h;
    if ( app.g_viewport.h < ZOOM_MIN_H ) app.g_viewport.h = ZOOM_MIN_H;
    if ( app.g_viewport.h > ZOOM_MAX_H ) app.g_viewport.h = ZOOM_MAX_H;
    app.g_viewport.w = app.g_viewport.h * aspect;
  }

  /* Always re-center viewport on player */
  app.g_viewport.x = player.world_x;
  app.g_viewport.y = player.world_y;

  a_DoWidget();
}

static void gs_DrawTopBar( void )
{
  aContainerWidget_t* tb = a_GetContainerFromWidget( "top_bar" );
  aRectf_t r = tb->rect;

  /* Background + border */
  a_DrawFilledRect( r, PANEL_BG );
  a_DrawRect( r, PANEL_FG );

  aTextStyle_t ts = a_default_text_style;
  ts.bg = (aColor_t){ 0, 0, 0, 0 };

  float tx = r.x + TB_PAD_X;
  float ty = r.y + TB_PAD_Y;

  /* Player name — left side, larger */
  ts.fg = white;
  ts.scale = TB_NAME_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  a_DrawText( player.name, (int)tx, (int)ty, ts );

  /* Measure name width to place stats right after it */
  float name_w = strlen( player.name ) * 8.0f * TB_NAME_SCALE;
  float sx = tx + name_w + TB_SECTION_GAP;

  /* Stats — flexed right of name, smaller */
  ts.scale = TB_STAT_SCALE;
  ts.align = TEXT_ALIGN_LEFT;
  char buf[32];

  /* HP — color based on percentage */
  float hp_pct = ( player.max_hp > 0 ) ? (float)player.hp / player.max_hp : 0;
  if ( hp_pct > 0.5f )       ts.fg = white;
  else if ( hp_pct > 0.25f ) ts.fg = yellow;
  else                        ts.fg = (aColor_t){ 255, 60, 60, 255 };

  snprintf( buf, sizeof( buf ), "HP: %d/%d", player.hp, player.max_hp );
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DMG */
  snprintf( buf, sizeof( buf ), "DMG: %d", player.base_damage );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );
  sx += strlen( buf ) * 8.0f * TB_STAT_SCALE + TB_STAT_GAP;

  /* DEF */
  snprintf( buf, sizeof( buf ), "DEF: %d", player.defense );
  ts.fg = white;
  a_DrawText( buf, (int)sx, (int)( ty + 4 ), ts );

  /* Settings[ESC] — far right */
  ts.scale = TB_ESC_SCALE;
  ts.fg = (aColor_t){ 160, 160, 160, 255 };
  ts.align = TEXT_ALIGN_RIGHT;
  a_DrawText( "Settings[ESC]", (int)( r.x + r.w - TB_PAD_X ), (int)( ty + 6 ), ts );
}

static void gs_DrawInventory( void )
{
  aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
  aRectf_t r = ip->rect;

  float grid_y = r.y + INV_TITLE_H;
  float grid_w = r.w - INV_PAD * 2;
  float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
  float cell_w = grid_w / INV_COLS;
  float cell_h = grid_h / INV_ROWS;
  float cell = cell_w < cell_h ? cell_w : cell_h;

  /* Center the grid within the panel */
  float total_grid_w = cell * INV_COLS + 4 * ( INV_COLS - 1 );
  float total_grid_h = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
  float ox = ( grid_w - total_grid_w ) / 2.0f;
  float oy = ( grid_h - total_grid_h ) / 2.0f;

  for ( int row = 0; row < INV_ROWS; row++ )
  {
    for ( int col = 0; col < INV_COLS; col++ )
    {
      int idx = row * INV_COLS + col;
      float cx = r.x + INV_PAD + ox + col * ( cell + 4 );
      float cy = grid_y + oy + row * ( cell + 2 );
      aRectf_t cr = { cx, cy, cell - 1, cell - 1 };

      a_DrawFilledRect( cr, CELL_BG );
      a_DrawRect( cr, CELL_FG );

      /* Draw item in cell */
      InvSlot_t* slot = &player.inventory[idx];
      if ( slot->type == INV_CONSUMABLE && slot->index < g_num_consumables )
      {
        ConsumableInfo_t* c = &g_consumables[slot->index];
        float isz = cell * 0.6f;
        float ix = cx + ( cell - 1 - isz ) / 2.0f;
        float iy = cy + ( cell - 1 - isz ) / 2.0f;
        DrawImageOrGlyph( c->image, c->glyph, c->color, ix, iy, isz );
      }
      else if ( slot->type == INV_EQUIPMENT && slot->index < g_num_equipment )
      {
        EquipmentInfo_t* e = &g_equipment[slot->index];
        float isz = cell * 0.6f;
        float ix = cx + ( cell - 1 - isz ) / 2.0f;
        float iy = cy + ( cell - 1 - isz ) / 2.0f;
        DrawImageOrGlyph( NULL, e->glyph, e->color, ix, iy, isz );
      }

      /* Cursor highlight */
      if ( ui_focus == 1 && player.inv_focused && idx == player.inv_cursor )
        a_DrawRect( cr, inv_action_open ? GOLD : white );
    }
  }
}

static void gs_DrawEquipment( void )
{
  aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
  aRectf_t r = kp->rect;

  float ey = r.y + EQ_TITLE_H;

  aTextStyle_t ts = a_default_text_style;
  ts.bg = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = EQ_TEXT_SCALE;
  ts.align = TEXT_ALIGN_LEFT;

  for ( int i = 0; i < EQUIP_SLOTS; i++ )
  {
    float row_y = ey + i * ( EQ_ROW_H + EQ_PAD );
    aRectf_t row_rect = { r.x + EQ_PAD, row_y, r.w - EQ_PAD * 2, EQ_ROW_H };

    a_DrawFilledRect( row_rect, CELL_BG );
    a_DrawRect( row_rect, CELL_FG );

    /* Slot label */
    ts.fg = GREY;
    a_DrawText( eq_slot_labels[i], (int)( row_rect.x + 4 ), (int)( row_y + 8 ), ts );

    /* Item in slot */
    if ( player.equipment[i] >= 0 && player.equipment[i] < g_num_equipment )
    {
      EquipmentInfo_t* e = &g_equipment[player.equipment[i]];

      /* Glyph */
      float ix = row_rect.x + 40;
      float iy = row_y + ( EQ_ROW_H - EQ_ICON_SIZE ) / 2.0f;
      DrawImageOrGlyph( NULL, e->glyph, e->color, ix, iy, EQ_ICON_SIZE );

      /* Name */
      ts.fg = white;
      a_DrawText( e->name, (int)( ix + EQ_ICON_SIZE + 6 ), (int)( row_y + 8 ), ts );
    }
    else
    {
      ts.fg = (aColor_t){ 80, 80, 80, 255 };
      a_DrawText( "---", (int)( row_rect.x + 44 ), (int)( row_y + 8 ), ts );
    }

    /* Cursor highlight */
    if ( ui_focus == 1 && !player.inv_focused && i == player.equip_cursor )
      a_DrawRect( row_rect, eq_action_open ? GOLD : white );
  }
}

static void gs_Draw( float dt )
{
  /* Draw manual backgrounds for boxed:0 panels */

  /* Top bar */
  gs_DrawTopBar();

  /* Game viewport — shrink 1px on right so it doesn't overlap right panels */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t vr = { vp->rect.x, vp->rect.y, vp->rect.w - 1, vp->rect.h };
    a_DrawFilledRect( vr, (aColor_t){ 0, 0, 0, 255 } );
    a_DrawRect( vr, PANEL_FG );
  }

  /* Inventory panel */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aColor_t ic = PANEL_FG;
    if ( ui_focus == 1 && player.inv_focused )
      ic = inv_action_open ? GOLD : white;
    a_DrawRect( ip->rect, ic );
  }

  /* Key items panel */
  {
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    aColor_t kc = PANEL_FG;
    if ( ui_focus == 1 && !player.inv_focused )
      kc = eq_action_open ? GOLD : white;
    a_DrawRect( kp->rect, kc );
  }

  /* Console panel */
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    a_DrawFilledRect( cp->rect, PANEL_BG );
    a_DrawRect( cp->rect, PANEL_FG );
    ConsoleDraw( &console, cp->rect );
  }

  /* Highlight active panel title in gold */
  {
    aWidget_t* inv_t = a_GetWidget( "inv_title" );
    aWidget_t* key_t = a_GetWidget( "key_title" );
    if ( inv_t ) inv_t->fg = ( ui_focus == 1 && player.inv_focused && inv_action_open ) ? GOLD : white;
    if ( key_t ) key_t->fg = ( ui_focus == 1 && !player.inv_focused && eq_action_open ) ? GOLD : white;
  }

  a_DrawWidgets();

  /* [Tab] hint — gold when inventory focused, grey otherwise */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aTextStyle_t ht = a_default_text_style;
    ht.bg = (aColor_t){ 0, 0, 0, 0 };
    ht.fg = ui_focus == 1 ? GOLD : (aColor_t){ 160, 160, 160, 255 };
    ht.scale = 1.0f;
    ht.align = TEXT_ALIGN_CENTER;
    a_DrawText( "[Tab]", (int)( ip->rect.x + ip->rect.w / 2 ), (int)( ip->rect.y + 6 ), ht );
  }

  gs_DrawInventory();
  gs_DrawEquipment();

  /* Equipment tooltip — show when equipment panel is focused and slot has an item */
  if ( ui_focus == 1 && !player.inv_focused &&
       player.equip_cursor >= 0 && player.equip_cursor < EQUIP_SLOTS &&
       player.equipment[player.equip_cursor] >= 0 &&
       player.equipment[player.equip_cursor] < g_num_equipment )
  {
    EquipmentInfo_t* e = &g_equipment[player.equipment[player.equip_cursor]];
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );

    /* Position modal to the left of the equipment panel */
    float mx = kp->rect.x - EQ_MODAL_W - 8;
    float my = kp->rect.y + EQ_TITLE_H + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
    if ( my + EQ_MODAL_H > kp->rect.y + kp->rect.h ) my = kp->rect.y + kp->rect.h - EQ_MODAL_H;

    a_DrawFilledRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, (aColor_t){ 0, 0, 0, 255 } );
    a_DrawRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, e->color );

    float ty = my + EQ_MODAL_PAD_Y;
    float tx = mx + EQ_MODAL_PAD_X;

    aTextStyle_t ts = a_default_text_style;
    ts.bg = (aColor_t){ 0, 0, 0, 0 };

    /* Name */
    ts.fg = e->color;
    ts.scale = EQ_MODAL_NAME_S;
    a_DrawText( e->name, (int)tx, (int)ty, ts );
    ty += EQ_MODAL_LINE_LG;

    /* Stats */
    char buf[128];
    ts.fg = white;
    ts.scale = EQ_MODAL_TEXT_S;
    if ( e->damage > 0 )
    {
      snprintf( buf, sizeof( buf ), "DMG: +%d", e->damage );
      a_DrawText( buf, (int)tx, (int)ty, ts );
      ty += EQ_MODAL_LINE_SM;
    }
    if ( e->defense > 0 )
    {
      snprintf( buf, sizeof( buf ), "DEF: +%d", e->defense );
      a_DrawText( buf, (int)tx, (int)ty, ts );
      ty += EQ_MODAL_LINE_SM;
    }
    if ( strcmp( e->effect, "none" ) != 0 )
    {
      ts.fg = yellow;
      snprintf( buf, sizeof( buf ), "%s (%d)", e->effect, e->effect_value );
      a_DrawText( buf, (int)tx, (int)ty, ts );
      ty += EQ_MODAL_LINE_MD;
    }
    else
    {
      ty += EQ_MODAL_LINE_SM;
    }

    /* Description */
    ts.fg = (aColor_t){ 180, 180, 180, 255 };
    ts.scale = EQ_MODAL_DESC_S;
    ts.wrap_width = (int)( EQ_MODAL_W - EQ_MODAL_PAD_X * 2 );
    a_DrawText( e->description, (int)tx, (int)ty, ts );

    /* Action menu — to the left of tooltip, or right of equip panel if off-screen */
    if ( eq_action_open )
    {
      float ah = EQ_ACTION_COUNT * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD ) - EQ_ACTION_PAD;
      float ax = mx - EQ_ACTION_W - 4;            /* left of tooltip */
      float ay = my;                               /* align top with tooltip */

      /* If it would go off the left edge, flip to right of equipment panel */
      if ( ax < 0 )
        ax = kp->rect.x + kp->rect.w + 4;

      a_DrawFilledRect( (aRectf_t){ ax, ay, EQ_ACTION_W, ah }, (aColor_t){ 20, 20, 20, 255 } );
      a_DrawRect( (aRectf_t){ ax, ay, EQ_ACTION_W, ah }, CELL_FG );

      aTextStyle_t ats = a_default_text_style;
      ats.bg = (aColor_t){ 0, 0, 0, 0 };
      ats.scale = EQ_MODAL_TEXT_S;
      ats.align = TEXT_ALIGN_LEFT;

      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = ay + i * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );
        aRectf_t row = { ax + 2, ry, EQ_ACTION_W - 4, EQ_ACTION_ROW_H };

        if ( i == eq_action_cursor )
        {
          a_DrawFilledRect( row, (aColor_t){ 40, 40, 40, 255 } );
          a_DrawRect( row, GOLD );
          ats.fg = GOLD;
        }
        else
        {
          ats.fg = white;
        }

        a_DrawText( eq_action_labels[i], (int)( row.x + 8 ), (int)( ry + 5 ), ats );
      }
    }
  }

  /* Inventory tooltip — show when inventory panel is focused and slot has an item */
  if ( ui_focus == 1 && player.inv_focused &&
       player.inv_cursor >= 0 && player.inv_cursor < MAX_INVENTORY &&
       player.inventory[player.inv_cursor].type != INV_EMPTY )
  {
    InvSlot_t* slot = &player.inventory[player.inv_cursor];
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );

    /* Compute cursor cell position for aligning the modal */
    aRectf_t r = ip->rect;
    float grid_y = r.y + INV_TITLE_H;
    float grid_w = r.w - INV_PAD * 2;
    float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
    float cell_w = grid_w / INV_COLS;
    float cell_h = grid_h / INV_ROWS;
    float cell = cell_w < cell_h ? cell_w : cell_h;
    float total_gh = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
    float oy = ( grid_h - total_gh ) / 2.0f;
    int cur_row = player.inv_cursor / INV_COLS;
    float cell_y = grid_y + oy + cur_row * ( cell + 2 );

    /* Position modal to the left of the inventory panel */
    float mx = ip->rect.x - EQ_MODAL_W - 8;
    float my = cell_y;
    /* Clamp so it doesn't go off bottom of screen */
    if ( my + EQ_MODAL_H > ip->rect.y + ip->rect.h )
      my = ip->rect.y + ip->rect.h - EQ_MODAL_H;

    const char* item_name = NULL;

    if ( slot->type == INV_EQUIPMENT && slot->index < g_num_equipment )
    {
      EquipmentInfo_t* e = &g_equipment[slot->index];
      item_name = e->name;

      a_DrawFilledRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, (aColor_t){ 0, 0, 0, 255 } );
      a_DrawRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, e->color );

      float ty = my + EQ_MODAL_PAD_Y;
      float tx = mx + EQ_MODAL_PAD_X;
      aTextStyle_t ts = a_default_text_style;
      ts.bg = (aColor_t){ 0, 0, 0, 0 };

      ts.fg = e->color;
      ts.scale = EQ_MODAL_NAME_S;
      a_DrawText( e->name, (int)tx, (int)ty, ts );
      ty += EQ_MODAL_LINE_LG;

      char buf[128];
      ts.fg = white;
      ts.scale = EQ_MODAL_TEXT_S;
      if ( e->damage > 0 )
      {
        snprintf( buf, sizeof( buf ), "DMG: +%d", e->damage );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += EQ_MODAL_LINE_SM;
      }
      if ( e->defense > 0 )
      {
        snprintf( buf, sizeof( buf ), "DEF: +%d", e->defense );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += EQ_MODAL_LINE_SM;
      }
      if ( strcmp( e->effect, "none" ) != 0 )
      {
        ts.fg = yellow;
        snprintf( buf, sizeof( buf ), "%s (%d)", e->effect, e->effect_value );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += EQ_MODAL_LINE_MD;
      }
      else
      {
        ty += EQ_MODAL_LINE_SM;
      }

      ts.fg = (aColor_t){ 180, 180, 180, 255 };
      ts.scale = EQ_MODAL_DESC_S;
      ts.wrap_width = (int)( EQ_MODAL_W - EQ_MODAL_PAD_X * 2 );
      a_DrawText( e->description, (int)tx, (int)ty, ts );
    }
    else if ( slot->type == INV_CONSUMABLE && slot->index < g_num_consumables )
    {
      ConsumableInfo_t* c = &g_consumables[slot->index];
      item_name = c->name;

      a_DrawFilledRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, (aColor_t){ 0, 0, 0, 255 } );
      a_DrawRect( (aRectf_t){ mx, my, EQ_MODAL_W, EQ_MODAL_H }, c->color );

      float ty = my + EQ_MODAL_PAD_Y;
      float tx = mx + EQ_MODAL_PAD_X;
      aTextStyle_t ts = a_default_text_style;
      ts.bg = (aColor_t){ 0, 0, 0, 0 };

      ts.fg = c->color;
      ts.scale = EQ_MODAL_NAME_S;
      a_DrawText( c->name, (int)tx, (int)ty, ts );
      ty += EQ_MODAL_LINE_LG;

      char buf[128];
      ts.fg = white;
      ts.scale = EQ_MODAL_TEXT_S;
      if ( c->bonus_damage > 0 )
      {
        snprintf( buf, sizeof( buf ), "DMG: +%d", c->bonus_damage );
        a_DrawText( buf, (int)tx, (int)ty, ts );
        ty += EQ_MODAL_LINE_SM;
      }
      if ( strcmp( c->effect, "none" ) != 0 && strlen( c->effect ) > 0 )
      {
        ts.fg = yellow;
        a_DrawText( c->effect, (int)tx, (int)ty, ts );
        ty += EQ_MODAL_LINE_MD;
      }
      else
      {
        ty += EQ_MODAL_LINE_SM;
      }

      ts.fg = (aColor_t){ 180, 180, 180, 255 };
      ts.scale = EQ_MODAL_DESC_S;
      ts.wrap_width = (int)( EQ_MODAL_W - EQ_MODAL_PAD_X * 2 );
      a_DrawText( c->description, (int)tx, (int)ty, ts );
    }

    /* Inventory action menu */
    if ( inv_action_open && item_name )
    {
      float ah = EQ_ACTION_COUNT * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD ) - EQ_ACTION_PAD;
      float ax = mx - EQ_ACTION_W - 4;
      if ( ax < 0 ) ax = ip->rect.x + ip->rect.w + 4;
      float ay = my;

      a_DrawFilledRect( (aRectf_t){ ax, ay, EQ_ACTION_W, ah }, (aColor_t){ 20, 20, 20, 255 } );
      a_DrawRect( (aRectf_t){ ax, ay, EQ_ACTION_W, ah }, CELL_FG );

      aTextStyle_t ats = a_default_text_style;
      ats.bg = (aColor_t){ 0, 0, 0, 0 };
      ats.scale = EQ_MODAL_TEXT_S;
      ats.align = TEXT_ALIGN_LEFT;

      const char* inv_labels[EQ_ACTION_COUNT];
      inv_labels[0] = ( slot->type == INV_EQUIPMENT ) ? "Equip" : "Use";
      inv_labels[1] = "Look";

      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = ay + i * ( EQ_ACTION_ROW_H + EQ_ACTION_PAD );
        aRectf_t row = { ax + 2, ry, EQ_ACTION_W - 4, EQ_ACTION_ROW_H };

        if ( i == inv_action_cursor )
        {
          a_DrawFilledRect( row, (aColor_t){ 40, 40, 40, 255 } );
          a_DrawRect( row, GOLD );
          ats.fg = GOLD;
        }
        else
        {
          ats.fg = white;
        }

        a_DrawText( inv_labels[i], (int)( row.x + 8 ), (int)( ry + 5 ), ats );
      }
    }
  }

  /* Player character — always centered in game_viewport panel */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t clip = { vp->rect.x + 1, vp->rect.y + 1, vp->rect.w - 3, vp->rect.h - 2 };
    a_SetClipRect( clip );

    float scale = vp->rect.h / app.g_viewport.h;
    float sprite_sz = 16.0f * scale;
    float cx = vp->rect.x + vp->rect.w / 2.0f - sprite_sz / 2.0f;
    float cy = vp->rect.y + vp->rect.h / 2.0f - sprite_sz / 2.0f;

    if ( player.image && settings.gfx_mode == GFX_IMAGE )
      a_BlitRect( player.image, NULL, &(aRectf_t){ cx, cy, sprite_sz, sprite_sz }, 1.0f );
    else
      a_DrawFilledRect( (aRectf_t){ cx, cy, sprite_sz, sprite_sz }, white );

    a_DisableClipRect();
  }
}
