#include <stdio.h>
#include <string.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "items.h"
#include "draw_utils.h"
#include "context_menu.h"
#include "game_events.h"
#include "inventory_ui.h"

/* Panel colors */
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

static const char* eq_slot_labels[EQUIP_SLOTS] = { "WPN", "ARM", "TRK", "TRK" };
static const char* eq_action_labels[EQ_ACTION_COUNT] = { "Unequip", "Look" };

static int eq_action_open   = 0;
static int eq_action_cursor = 0;

static int inv_action_open   = 0;
static int inv_action_cursor = 0;

static int ui_focus = 0;        /* 0 = game viewport, 1 = inventory panels */
static int show_item_hover = 1; /* 0 = suppress cursor highlight + tooltip */

static aSoundEffect_t* sfx_move;
static aSoundEffect_t* sfx_click;

/* Intro transition offsets */
static float ui_offset_x = 0.0f;
static float ui_alpha     = 1.0f;

/* ------------------------------------------------------------------ */

void InventoryUIInit( aSoundEffect_t* move, aSoundEffect_t* click )
{
  sfx_move  = move;
  sfx_click = click;
  ui_focus  = 0;
  eq_action_open  = 0;
  inv_action_open = 0;
}

int InventoryUICloseMenus( void )
{
  if ( eq_action_open || inv_action_open )
  {
    eq_action_open  = 0;
    inv_action_open = 0;
    return 1;
  }
  return 0;
}

int InventoryUIFocused( void )
{
  return ui_focus;
}

void InventoryUIUnfocus( void )
{
  ui_focus = 0;
  show_item_hover = 0;
}

void InventoryUISetIntroOffset( float x_offset, float alpha )
{
  ui_offset_x = x_offset;
  ui_alpha    = alpha;
}

/* ------------------------------------------------------------------ */

int InventoryUILogic( int mouse_moved )
{
  /* --- Equipment action menu input --- */
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
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      eq_action_cursor++;
      if ( eq_action_cursor >= EQ_ACTION_COUNT ) eq_action_cursor = 0;
      a_AudioPlaySound( sfx_move, NULL );
    }

    /* Scroll wheel — navigate action menu */
    if ( app.mouse.wheel != 0 )
    {
      eq_action_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
      if ( eq_action_cursor < 0 ) eq_action_cursor = EQ_ACTION_COUNT - 1;
      if ( eq_action_cursor >= EQ_ACTION_COUNT ) eq_action_cursor = 0;
      a_AudioPlaySound( sfx_move, NULL );
      app.mouse.wheel = 0;
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
      float ax = modal_x - CTX_MENU_W - 4;
      if ( ax < 0 ) ax = kp->rect.x + kp->rect.w + 4;
      float ay = modal_y;
      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = ay + i * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
        if ( PointInRect( mx, my, ax, ry, CTX_MENU_W, CTX_MENU_ROW_H ) )
        {
          if ( i != eq_action_cursor )
            a_AudioPlaySound( sfx_move, NULL );
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
      app.mouse.pressed = 0;

      int mx = app.mouse.x;
      int my = app.mouse.y;
      aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );

      float modal_x = kp->rect.x - EQ_MODAL_W - 8;
      float modal_y = kp->rect.y + EQ_TITLE_H + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
      if ( modal_y + EQ_MODAL_H > kp->rect.y + kp->rect.h ) modal_y = kp->rect.y + kp->rect.h - EQ_MODAL_H;
      float action_h = EQ_ACTION_COUNT * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
      float ax = modal_x - CTX_MENU_W - 4;
      if ( ax < 0 ) ax = kp->rect.x + kp->rect.w + 4;
      float ay = modal_y;

      if ( PointInRect( mx, my, ax, ay, CTX_MENU_W, action_h ) )
      {
        exec = 1;
      }
      else
      {
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
      a_AudioPlaySound( sfx_click, NULL );
      if ( eq_action_cursor == 0 ) /* Unequip */
      {
        int inv_slot = InventoryAdd( INV_EQUIPMENT, eq_idx );
        if ( inv_slot >= 0 )
        {
          player.equipment[player.equip_cursor] = -1;
          GameEvent( EVT_UNEQUIP, eq_idx );
        }
      }
      else if ( eq_action_cursor == 1 ) /* Look */
      {
        GameEvent( EVT_LOOK_EQUIPMENT, eq_idx );
      }
      eq_action_open = 0;
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
    return 1;
  }

  /* --- Inventory action menu input --- */
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
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      inv_action_cursor++;
      if ( inv_action_cursor >= EQ_ACTION_COUNT ) inv_action_cursor = 0;
      a_AudioPlaySound( sfx_move, NULL );
    }

    /* Scroll wheel — navigate action menu */
    if ( app.mouse.wheel != 0 )
    {
      inv_action_cursor += ( app.mouse.wheel < 0 ) ? 1 : -1;
      if ( inv_action_cursor < 0 ) inv_action_cursor = EQ_ACTION_COUNT - 1;
      if ( inv_action_cursor >= EQ_ACTION_COUNT ) inv_action_cursor = 0;
      a_AudioPlaySound( sfx_move, NULL );
      app.mouse.wheel = 0;
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

      float modal_y = cell_y;
      if ( modal_y + EQ_MODAL_H > ip->rect.y + ip->rect.h )
        modal_y = ip->rect.y + ip->rect.h - EQ_MODAL_H;

      float modal_x = ip->rect.x - EQ_MODAL_W - 8;
      float amx = modal_x - CTX_MENU_W - 4;
      if ( amx < 0 ) amx = ip->rect.x + ip->rect.w + 4;
      float amy = modal_y;

      for ( int i = 0; i < EQ_ACTION_COUNT; i++ )
      {
        float ry = amy + i * ( CTX_MENU_ROW_H + CTX_MENU_PAD );
        if ( PointInRect( mx, my, amx, ry, CTX_MENU_W, CTX_MENU_ROW_H ) )
        {
          if ( i != inv_action_cursor )
            a_AudioPlaySound( sfx_move, NULL );
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

      float modal_y = cell_y;
      if ( modal_y + EQ_MODAL_H > ip->rect.y + ip->rect.h )
        modal_y = ip->rect.y + ip->rect.h - EQ_MODAL_H;

      float modal_x = ip->rect.x - EQ_MODAL_W - 8;
      float amx = modal_x - CTX_MENU_W - 4;
      if ( amx < 0 ) amx = ip->rect.x + ip->rect.w + 4;
      float amy = modal_y;
      float action_h = EQ_ACTION_COUNT * ( CTX_MENU_ROW_H + CTX_MENU_PAD );

      if ( PointInRect( mx, my, amx, amy, CTX_MENU_W, action_h ) )
      {
        exec = 1;
      }
      else
      {
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
      a_AudioPlaySound( sfx_click, NULL );
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
              int new_idx = slot->index;
              player.equipment[eq_slot] = new_idx;
              slot->type = INV_EQUIPMENT;
              slot->index = old;
              GameEventSwap( new_idx, old );
            }
            else
            {
              player.equipment[eq_slot] = slot->index;
              slot->type = INV_EMPTY;
              slot->index = 0;
              GameEvent( EVT_EQUIP, player.equipment[eq_slot] );
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
    return 1;
  }

  /* --- Tab — toggle game viewport / inventory panels --- */
  if ( app.keyboard[SDL_SCANCODE_TAB] == 1 )
  {
    app.keyboard[SDL_SCANCODE_TAB] = 0;
    ui_focus = !ui_focus;
    if ( ui_focus == 1 ) { player.inv_focused = 1; show_item_hover = 1; }
    a_AudioPlaySound( sfx_move, NULL );
  }

  /* --- Keyboard nav only when inventory is focused --- */
  if ( ui_focus == 1 && player.inv_focused )
  {
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      if ( player.inv_cursor < INV_COLS )
      {
        player.inv_focused = 0;
        player.equip_cursor = EQUIP_SLOTS - 1;
      }
      else
      {
        player.inv_cursor -= INV_COLS;
      }
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      if ( player.inv_cursor + INV_COLS >= MAX_INVENTORY )
      {
        player.inv_focused = 0;
        player.equip_cursor = 0;
      }
      else
      {
        player.inv_cursor += INV_COLS;
      }
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_A] == 1 || app.keyboard[SDL_SCANCODE_LEFT] == 1 )
    {
      app.keyboard[SDL_SCANCODE_A] = 0;
      app.keyboard[SDL_SCANCODE_LEFT] = 0;
      if ( player.inv_cursor % INV_COLS > 0 )
        player.inv_cursor--;
      else
        player.inv_cursor += INV_COLS - 1;
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_D] == 1 || app.keyboard[SDL_SCANCODE_RIGHT] == 1 )
    {
      app.keyboard[SDL_SCANCODE_D] = 0;
      app.keyboard[SDL_SCANCODE_RIGHT] = 0;
      if ( player.inv_cursor % INV_COLS < INV_COLS - 1 )
        player.inv_cursor++;
      else
        player.inv_cursor -= INV_COLS - 1;
      a_AudioPlaySound( sfx_move, NULL );
    }

    /* Space/Enter — open action menu on inventory item */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      InvSlot_t* s = &player.inventory[player.inv_cursor];
      if ( s->type != INV_EMPTY )
      {
        a_AudioPlaySound( sfx_click, NULL );
        inv_action_open = 1;
        inv_action_cursor = 0;
      }
    }
  }
  else if ( ui_focus == 1 ) /* Equipment navigation */
  {
    if ( app.keyboard[SDL_SCANCODE_W] == 1 || app.keyboard[SDL_SCANCODE_UP] == 1 )
    {
      app.keyboard[SDL_SCANCODE_W] = 0;
      app.keyboard[SDL_SCANCODE_UP] = 0;
      if ( player.equip_cursor == 0 )
      {
        player.inv_focused = 1;
        player.inv_cursor = MAX_INVENTORY - INV_COLS;
      }
      else
      {
        player.equip_cursor--;
      }
      a_AudioPlaySound( sfx_move, NULL );
    }
    if ( app.keyboard[SDL_SCANCODE_S] == 1 || app.keyboard[SDL_SCANCODE_DOWN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_S] = 0;
      app.keyboard[SDL_SCANCODE_DOWN] = 0;
      if ( player.equip_cursor >= EQUIP_SLOTS - 1 )
      {
        player.inv_focused = 1;
        player.inv_cursor = 0;
      }
      else
      {
        player.equip_cursor++;
      }
      a_AudioPlaySound( sfx_move, NULL );
    }

    /* Space/Enter — open action menu on equipped item */
    if ( app.keyboard[SDL_SCANCODE_SPACE] == 1 || app.keyboard[SDL_SCANCODE_RETURN] == 1 )
    {
      app.keyboard[SDL_SCANCODE_SPACE] = 0;
      app.keyboard[SDL_SCANCODE_RETURN] = 0;
      if ( player.equipment[player.equip_cursor] >= 0 )
      {
        a_AudioPlaySound( sfx_click, NULL );
        eq_action_open = 1;
        eq_action_cursor = 0;
      }
    }
  }

  /* --- Mouse hover + click — equipment rows --- */
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
            a_AudioPlaySound( sfx_move, NULL );
          player.equip_cursor = i;
          player.inv_focused = 0;
          ui_focus = 1;
          show_item_hover = 1;
        }

        /* Click opens action menu */
        if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT && player.equipment[i] >= 0 )
        {
          app.mouse.pressed = 0;
          a_AudioPlaySound( sfx_click, NULL );
          eq_action_open = 1;
          eq_action_cursor = 0;
        }
        break;
      }
    }
  }

  /* --- Mouse hover — inventory cells --- */
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
              a_AudioPlaySound( sfx_move, NULL );
            player.inv_cursor = idx;
            player.inv_focused = 1;
            ui_focus = 1;
            show_item_hover = 1;
          }

          /* Click opens action menu */
          if ( app.mouse.pressed && app.mouse.button == SDL_BUTTON_LEFT &&
               player.inventory[idx].type != INV_EMPTY )
          {
            app.mouse.pressed = 0;
            a_AudioPlaySound( sfx_click, NULL );
            inv_action_open = 1;
            inv_action_cursor = 0;
          }
          goto hover_done;
        }
      }
    }
    hover_done:;
  }

  /* --- Click outside inventory/equipment panels — switch focus back to game --- */
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

  return 0;
}

/* ------------------------------------------------------------------ */

static void DrawInventoryGrid( void )
{
  aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
  aRectf_t r = ip->rect;
  r.x += ui_offset_x;

  float grid_y = r.y + INV_TITLE_H;
  float grid_w = r.w - INV_PAD * 2;
  float grid_h = r.h - INV_TITLE_H - INV_PAD * 2;
  float cell_w = grid_w / INV_COLS;
  float cell_h = grid_h / INV_ROWS;
  float cell = cell_w < cell_h ? cell_w : cell_h;

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
      if ( ui_focus == 1 && show_item_hover && player.inv_focused && idx == player.inv_cursor )
        a_DrawRect( cr, inv_action_open ? GOLD : white );
    }
  }
}

static void DrawEquipmentRows( void )
{
  aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
  aRectf_t r = kp->rect;
  r.x += ui_offset_x;

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

    ts.fg = GREY;
    a_DrawText( eq_slot_labels[i], (int)( row_rect.x + 4 ), (int)( row_y + 8 ), ts );

    if ( player.equipment[i] >= 0 && player.equipment[i] < g_num_equipment )
    {
      EquipmentInfo_t* e = &g_equipment[player.equipment[i]];

      float ix = row_rect.x + 40;
      float iy = row_y + ( EQ_ROW_H - EQ_ICON_SIZE ) / 2.0f;
      DrawImageOrGlyph( NULL, e->glyph, e->color, ix, iy, EQ_ICON_SIZE );

      ts.fg = white;
      a_DrawText( e->name, (int)( ix + EQ_ICON_SIZE + 6 ), (int)( row_y + 8 ), ts );
    }
    else
    {
      ts.fg = (aColor_t){ 80, 80, 80, 255 };
      a_DrawText( "---", (int)( row_rect.x + 44 ), (int)( row_y + 8 ), ts );
    }

    /* Cursor highlight */
    if ( ui_focus == 1 && show_item_hover && !player.inv_focused && i == player.equip_cursor )
      a_DrawRect( row_rect, eq_action_open ? GOLD : white );
  }
}

/* ------------------------------------------------------------------ */

void InventoryUIDraw( void )
{
  /* Inventory panel border */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aRectf_t ir = ip->rect;
    ir.x += ui_offset_x;
    aColor_t ic = PANEL_FG;
    ic.a = (int)( ic.a * ui_alpha );
    if ( ui_focus == 1 && player.inv_focused )
      ic = inv_action_open ? GOLD : white;
    a_DrawRect( ir, ic );
  }

  /* Equipment panel border */
  {
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    aRectf_t kr = kp->rect;
    kr.x += ui_offset_x;
    aColor_t kc = PANEL_FG;
    kc.a = (int)( kc.a * ui_alpha );
    if ( ui_focus == 1 && !player.inv_focused )
      kc = eq_action_open ? GOLD : white;
    a_DrawRect( kr, kc );
  }

  /* Title highlights */
  {
    aWidget_t* inv_t = a_GetWidget( "inv_title" );
    aWidget_t* key_t = a_GetWidget( "key_title" );
    if ( inv_t ) inv_t->fg = ( ui_focus == 1 && player.inv_focused && inv_action_open ) ? GOLD : white;
    if ( key_t ) key_t->fg = ( ui_focus == 1 && !player.inv_focused && eq_action_open ) ? GOLD : white;
  }

  /* [Tab] hint */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aTextStyle_t ht = a_default_text_style;
    ht.bg = (aColor_t){ 0, 0, 0, 0 };
    ht.fg = ui_focus == 1 ? GOLD : (aColor_t){ 160, 160, 160, 255 };
    ht.fg.a = (int)( ht.fg.a * ui_alpha );
    ht.scale = 1.0f;
    ht.align = TEXT_ALIGN_CENTER;
    a_DrawText( "[Tab]", (int)( ip->rect.x + ui_offset_x + ip->rect.w / 2 ), (int)( ip->rect.y + 6 ), ht );
  }

  DrawInventoryGrid();
  DrawEquipmentRows();

  /* Equipment tooltip */
  if ( ui_focus == 1 && show_item_hover && !player.inv_focused &&
       player.equip_cursor >= 0 && player.equip_cursor < EQUIP_SLOTS &&
       player.equipment[player.equip_cursor] >= 0 &&
       player.equipment[player.equip_cursor] < g_num_equipment )
  {
    EquipmentInfo_t* e = &g_equipment[player.equipment[player.equip_cursor]];
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );

    float mx = kp->rect.x - EQ_MODAL_W - 8;
    float my = kp->rect.y + EQ_TITLE_H + player.equip_cursor * ( EQ_ROW_H + EQ_PAD );
    if ( my + EQ_MODAL_H > kp->rect.y + kp->rect.h ) my = kp->rect.y + kp->rect.h - EQ_MODAL_H;

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

    /* Equipment action menu */
    if ( eq_action_open )
    {
      float ax = mx - CTX_MENU_W - 4;
      float ay = my;

      if ( ax < 0 )
        ax = kp->rect.x + kp->rect.w + 4;

      DrawContextMenu( ax, ay,
                       eq_action_labels, EQ_ACTION_COUNT,
                       eq_action_cursor );
    }
  }

  /* Inventory tooltip */
  if ( ui_focus == 1 && show_item_hover && player.inv_focused &&
       player.inv_cursor >= 0 && player.inv_cursor < MAX_INVENTORY &&
       player.inventory[player.inv_cursor].type != INV_EMPTY )
  {
    InvSlot_t* slot = &player.inventory[player.inv_cursor];
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
    int cur_row = player.inv_cursor / INV_COLS;
    float cell_y = grid_y + oy + cur_row * ( cell + 2 );

    float mx = ip->rect.x - EQ_MODAL_W - 8;
    float my = cell_y;
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
      float ax = mx - CTX_MENU_W - 4;
      if ( ax < 0 ) ax = ip->rect.x + ip->rect.w + 4;
      float ay = my;

      const char* inv_labels[EQ_ACTION_COUNT];
      inv_labels[0] = ( slot->type == INV_EQUIPMENT ) ? "Equip" : "Use";
      inv_labels[1] = "Look";

      DrawContextMenu( ax, ay,
                       inv_labels, EQ_ACTION_COUNT,
                       inv_action_cursor );
    }
  }
}
