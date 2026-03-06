#include <math.h>
#include <Archimedes.h>

#include "defines.h"
#include "player.h"
#include "draw_utils.h"
#include "console.h"
#include "game_events.h"
#include "inventory_ui.h"
#include "transitions.h"
#include "sound_manager.h"
#include "world.h"
#include "game_viewport.h"
#include "game_scene.h"
#include "main_menu.h"
#include "enemies.h"
#include "combat.h"
#include "combat_vfx.h"
#include "movement.h"
#include "tile_actions.h"
#include "look_mode.h"
#include "hud.h"
#include "visibility.h"
#include "npc.h"
#include "dialogue.h"
#include "dialogue_ui.h"
#include "ground_items.h"
#include "items.h"
#include "dungeon.h"
#include "quest_tracker.h"
#include "objects.h"
#include "maps.h"
#include "shop.h"
#include "shop_ui.h"
#include "poison_pool.h"
#include "placed_traps.h"
#include "spell_vfx.h"
#include "target_mode.h"
#include "pause_menu.h"
#include "game_over.h"
#include "room_enumerator.h"
#include "game_camera.h"
#include "game_turns.h"
#include "game_input.h"
#include "floor_cutscene.h"
#include "npc_relocate.h"
#include "dev_mode.h"
#include "bank.h"
#include "lore.h"
#include "dungeon_spawner.h"

static void gs_Logic( float );
static void gs_Draw( float );

/* Panel colors - palette */
#define PANEL_BG  (aColor_t){ 0x09, 0x0a, 0x14, 200 }
#define PANEL_FG  (aColor_t){ 0xc7, 0xcf, 0xcc, 255 }
#define GOLD      (aColor_t){ 0xde, 0x9e, 0x41, 255 }

static aTileset_t*  tileset = NULL;
static World_t*     world   = NULL;
static GameCamera_t camera;

static aSoundEffect_t sfx_move;
static aSoundEffect_t sfx_click;

static Console_t console;

static int hud_pause_clicked = 0;

/* Enemies */
static Enemy_t  enemies[MAX_ENEMIES];
static int      num_enemies = 0;

/* NPCs */
static NPC_t    npcs[MAX_NPCS];
static int      num_npcs = 0;

/* Ground items */
static GroundItem_t ground_items[MAX_GROUND_ITEMS];
static int          num_ground_items = 0;

#define HINT_FADE      0.4f
#define HINT_DURATION  2.5f

void GameSceneInit( void )
{
  app.delegate.logic = gs_Logic;
  app.delegate.draw  = gs_Draw;

  app.options.scale_factor = 1;

  a_WidgetsInit( "resources/widgets/game_scene.auf" );
  app.active_widget = a_GetWidget( "inv_panel" );

  /* ---- Build dungeon ---- */
  tileset = a_TilesetCreate( "resources/assets/tiles/level01tilemap.png", 16, 16 );
  world   = WorldCreate( DUNGEON_W, DUNGEON_H, 16, 16 );
  ConsoleInit( &console );
  ObjectsInit( &console );
  DungeonBuild( world );
  DungeonPlayerStart( &player.world_x, &player.world_y );

  /* Initialize game camera centered on player */
  camera = (GameCamera_t){ player.world_x, player.world_y, 64.0f };
  app.g_viewport = (aRectf_t){ 0, 0, 0, 0 };
  GameCameraInit( &camera );

  a_AudioLoadSound( "resources/soundeffects/menu_move.wav", &sfx_move );
  a_AudioLoadSound( "resources/soundeffects/menu_click.wav", &sfx_click );

  /* Init subsystems */
  MovementInit( world );
  TileActionsInit( world, &camera, &console, &sfx_move, &sfx_click );
  LookModeInit( world, &console, &sfx_move, &sfx_click );
  TargetModeInit( world, &console, &camera, &sfx_move, &sfx_click );
  InventoryUIInit( &sfx_move, &sfx_click );
  InventoryUISetGroundItems( ground_items, &num_ground_items,
                             world->tile_w, world->tile_h );

  VisibilityInit( world );

  GameEventsInit( &console );
  ConsolePush( &console, "Welcome, adventurer.", white );

  /* Enemies & combat */
  EnemiesLoadTypes();
  EnemiesInit( enemies, &num_enemies );
  EnemiesSetList( enemies, &num_enemies );
  CombatInit( &console );
  CombatSetEnemies( enemies, &num_enemies );
  CombatSetGroundItems( ground_items, &num_ground_items );
  GameEventsSetWorld( world, enemies, &num_enemies );
  CombatVFXInit();
  EnemyProjectileInit();
  EnemiesSetWorld( world );

  /* NPCs & dialogue */
  FlagsInit();
  /* Restore persistent lore discoveries as per-run flags */
  if ( LoreIsDiscovered( "shop_rats" ) )  FlagSet( "knows_shop_rats", 1 );
  BankInit( &console );
  DialogueLoadAll();
  EnemiesSetNPCs( npcs, &num_npcs );
  NPCsInit( npcs, &num_npcs );
  DialogueUIInit( &sfx_move, &sfx_click );

  /* Ground items */
  GroundItemsInit( ground_items, &num_ground_items );

  /* Shop */
  if ( g_current_floor >= 3 )
    ShopLoadPool( "resources/data/shops/floor_03_shop.duf" );
  else if ( g_current_floor == 2 )
    ShopLoadPool( "resources/data/shops/floor_02_shop.duf" );
  else
    ShopLoadPool( "resources/data/shops/floor_01_shop.duf" );
  ShopUIInit( &sfx_move, &sfx_click, &console );

  /* Poison pools */
  PoisonPoolInit( &console );

  /* Placed traps */
  PlacedTrapsInit( &console );

  /* Spell VFX */
  SpellVFXInit( world );

  /* Spawn all dungeon entities (items, NPCs, enemies) */
  DungeonSpawn( npcs, &num_npcs, enemies, &num_enemies,
                ground_items, &num_ground_items, world );
  FloorCutsceneRegister( npcs, num_npcs, enemies, num_enemies );

  TileActionsSetNPCs( npcs, &num_npcs );
  TileActionsSetGroundItems( ground_items, &num_ground_items );

  DungeonHandlerInit( world );

  GameTurnsInit( &console, &sfx_click, enemies, &num_enemies,
                 npcs, &num_npcs, ground_items, &num_ground_items, world );
  GameInputInit( world, &camera, &console,
                 enemies, &num_enemies, npcs, &num_npcs );
  DevModeInit( &console );
  DevModeSetNPCs( npcs, &num_npcs );
  NPCRelocateInit( npcs, &num_npcs );

  GameOverReset();
  SoundManagerPlayGame();
  TransitionIntroStart();
}

/* ===== Main logic loop ===== */

static void gs_Logic( float dt )
{
  GameInputFrameBegin( dt );

  GameTurnsTickHint( dt );

  /* Check HUD pause button click from previous draw frame */
  if ( hud_pause_clicked )
  {
    hud_pause_clicked = 0;
    if ( !PauseMenuActive() ) PauseMenuOpen();
  }

  if ( GameCameraIntro( dt ) )         return;
  if ( FloorCutsceneUpdate( dt ) )   { GameCameraFollow(); return; }
  if ( NPCRelocateUpdate( dt ) )     { GameCameraFollow(); return; }

  /* Game over - takes priority over everything */
  GameOverCheck( dt );
  if ( GameOverActive() )
  {
    int r = GameOverLogic( dt );
    if ( r == 2 ) { a_WidgetCacheFree(); MainMenuInit(); return; }
    GameCameraFollow();
    return;
  }

  if ( PauseMenuActive() )
  {
    int r = PauseMenuLogic();
    if ( r == 2 ) { a_WidgetCacheFree(); MainMenuInit(); return; }
    GameCameraFollow();
    return;
  }

  /* Stairway exit - check after dialogue closes */
  if ( !DialogueActive() && FlagGet( "stair_leave" ) )
  {
    FlagClear( "stair_leave" );
    a_WidgetCacheFree();
    MainMenuInit();
    return;
  }

  DungeonDeferredSpawns( npcs, num_npcs, enemies, &num_enemies, world );

  /* Stairway descend - transition to next floor */
  if ( !DialogueActive() && FlagGet( "stair_descend" ) )
  {
    FlagClear( "stair_descend" );
    /* Destroy treasure maps - they're floor-specific */
    for ( int i = 0; i < player.max_inventory; i++ )
      if ( player.inventory[i].type == INV_MAP )
        InventoryRemove( i );
    g_current_floor++;
    a_WidgetCacheFree();
    GameSceneInit();
    return;
  }

  if ( DevModeInput() )                { GameCameraFollow(); return; }
  if ( GameInputOverlays() )          { GameCameraFollow(); return; }
  if ( GameInputEsc() )               return;

  GameInputInventory();
  GameTurnsUpdateSystems( dt );
  GameTurnsHandleTurnEnd( dt, GameInputTurnSkipped() );
  GameInputClearTurnSkipped();
  if ( GameInputTargetMode() )        { GameCameraFollow(); return; }
  if ( GameInputLookMode() )          { GameCameraFollow(); return; }

  GameInputAutoMove();
  GameInputMovement();
  GameInputMouse();
  GameInputZoom();

  GameCameraFollow();
}

static void gs_Draw( float dt )
{
  (void)dt;

  /* Top bar */
  if ( HUDDrawTopBar( EnemiesInCombat( enemies, num_enemies ) ) )
    hud_pause_clicked = 1;

  /* Game viewport - shrink 1px on right so it doesn't overlap right panels */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t vr = { vp->rect.x, vp->rect.y, vp->rect.w - 1, vp->rect.h };
    float va = TransitionGetViewportAlpha();
    a_DrawFilledRect( vr, (aColor_t){ 0, 0, 0, (int)( 255 * va ) } );
    a_DrawRect( vr, (aColor_t){ 0x39, 0x4a, 0x50, (int)( 255 * va ) } );
  }

  /* Inventory panel background */
  {
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aRectf_t ir = ip->rect;
    ir.x += TransitionGetRightOX();
    a_DrawFilledRect( ir, PANEL_BG );
  }

  /* Equipment panel background */
  {
    aContainerWidget_t* kp = a_GetContainerFromWidget( "key_panel" );
    aRectf_t kr = kp->rect;
    kr.x += TransitionGetRightOX();
    a_DrawFilledRect( kr, PANEL_BG );
  }

  /* Console panel (dialogue/shop UI drawn later, on top of viewport) */
  if ( !DialogueActive() && !ShopUIActive() )
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    aRectf_t cr = cp->rect;
    cr.y += TransitionGetConsoleOY();
    aColor_t con_bg = PANEL_BG;
    con_bg.a = (int)( con_bg.a * TransitionGetUIAlpha() );
    aColor_t con_fg = PANEL_FG;
    con_fg.a = (int)( con_fg.a * TransitionGetUIAlpha() );
    a_DrawFilledRect( cr, con_bg );
    a_DrawRect( cr, con_fg );
    ConsoleDraw( &console, cr );
  }

  /* World + Player - clipped to game_viewport panel */
  {
    aContainerWidget_t* vp = a_GetContainerFromWidget( "game_viewport" );
    aRectf_t clip = { vp->rect.x + 1, vp->rect.y + 1, vp->rect.w - 3, vp->rect.h - 2 };
    a_SetClipRect( clip );

    aRectf_t vp_rect = vp->rect;

    /* Apply combat hit shake to camera */
    GameCamera_t draw_cam = camera;
    draw_cam.x += CombatShakeOX() + SpellVFXShakeOX();
    draw_cam.y += CombatShakeOY() + SpellVFXShakeOY();

    if ( world && tileset )
      GV_DrawWorld( vp_rect, &draw_cam, world, tileset, settings.gfx_mode == GFX_ASCII );

    /* Draw dungeon props (easel etc.) before darkness */
    DungeonDrawProps( vp_rect, &draw_cam, world, settings.gfx_mode );

    /* Draw shop rug + items on rug */
    ShopDrawRug( vp_rect, &draw_cam, world, settings.gfx_mode );
    ShopDrawItems( vp_rect, &draw_cam, world, settings.gfx_mode );

    /* Draw poison pools */
    PoisonPoolDrawAll( vp_rect, &draw_cam, world, settings.gfx_mode );

    /* Draw placed traps */
    PlacedTrapsDrawAll( vp_rect, &draw_cam, world, settings.gfx_mode );

    /* Draw totem aura overlay on affected floor tiles */
    CombatDrawTotemAura( vp_rect, &draw_cam, world );

    /* Draw ground items BEFORE enemies/darkness so they get dimmed */
    GroundItemsDrawAll( vp_rect, &draw_cam, ground_items, num_ground_items,
                        world, settings.gfx_mode );

    /* Draw enemies BEFORE darkness so they get dimmed too */
    EnemiesDrawAll( vp_rect, &draw_cam, enemies, num_enemies,
                    world, settings.gfx_mode );

    /* Draw NPCs before darkness */
    NPCsDrawAll( vp_rect, &draw_cam, npcs, num_npcs,
                  world, settings.gfx_mode );

    /* Darkness overlay - covers world + enemies, player drawn on top.
       fade param: 0 = all black (intro start), 1 = normal visibility. */
    GV_DrawDarkness( vp_rect, &draw_cam, world,
                     TransitionGetViewportAlpha() );

    /* Hover highlight */
    if ( GameInputHoverRow() >= 0 && GameInputHoverCol() >= 0 )
      GV_DrawTileOutline( vp_rect, &draw_cam,
                          GameInputHoverRow(), GameInputHoverCol(),
                          world->tile_w, world->tile_h,
                          (aColor_t){ 0xeb, 0xed, 0xe9, 80 } );

    /* Look mode cursor */
    LookModeDraw( vp_rect, &draw_cam );

    /* Target mode range + cursor */
    TargetModeDraw( vp_rect, &draw_cam, world );

    /* Tile action menu target highlight */
    if ( TileActionsIsOpen() )
      GV_DrawTileOutline( vp_rect, &draw_cam,
                          TileActionsGetRow(), TileActionsGetCol(),
                          world->tile_w, world->tile_h, GOLD );

    /* Skeleton telegraph + projectiles */
    EnemiesDrawTelegraph( vp_rect, &draw_cam, enemies, num_enemies, world );
    EnemyProjectileDraw( vp_rect, &draw_cam );

    /* Player sprite (drawn after darkness - always visible) */
    PlayerDraw( vp_rect, &draw_cam, settings.gfx_mode );

    /* Health bars - hide after 2 turns without taking damage */
    for ( int i = 0; i < num_enemies; i++ )
    {
      if ( !enemies[i].alive ) continue;
      if ( enemies[i].turns_since_hit >= 2 ) continue;
      int ex = (int)( enemies[i].world_x / world->tile_w );
      int ey = (int)( enemies[i].world_y / world->tile_h );
      if ( VisibilityGet( ex, ey ) < 0.01f ) continue;
      EnemyType_t* et = &g_enemy_types[enemies[i].type_idx];
      CombatVFXDrawHealthBar( vp_rect, &draw_cam,
                              enemies[i].world_x, enemies[i].world_y,
                              enemies[i].hp, et->hp );
    }
    if ( player.turns_since_hit < 2 )
      CombatVFXDrawHealthBar( vp_rect, &draw_cam,
                              player.world_x, player.world_y,
                              player.hp, player.max_hp );

    /* Floating damage numbers */
    CombatVFXDraw( vp_rect, &draw_cam );

    /* Spell VFX (projectiles, zap lines, tile flashes) */
    SpellVFXDraw( vp_rect, &draw_cam );

    /* Red flash overlay on hit (fires once, won't restart while active) */
    float flash = CombatFlashAlpha();
    if ( flash > 0.5f )
      a_DrawFilledRect( clip, (aColor_t){ 0xa5, 0x30, 0x30, (int)flash } );

    /* Spell colored screen flash */
    {
      float sf = SpellVFXFlashAlpha();
      if ( sf > 0.5f )
      {
        aColor_t sfc = SpellVFXFlashColor();
        sfc.a = (int)sf;
        a_DrawFilledRect( clip, sfc );
      }
    }

    /* Low HP red border pulse */
    if ( player.max_hp > 0 && player.hp > 0
         && player.hp <= player.max_hp / 4 )
    {
      static float pulse_t = 0;
      pulse_t += dt * 2.5f;
      float pulse = ( sinf( pulse_t ) + 1.0f ) * 0.5f;  /* 0..1 */
      int alpha = 80 + (int)( pulse * 120.0f );          /* 80..200 */
      aColor_t rc = { 0xb0, 0x20, 0x20, alpha };
      aRectf_t b = vp_rect;
      for ( int t = 0; t < 3; t++ )
      {
        a_DrawRect( b, rc );
        b.x += 1; b.y += 1; b.w -= 2; b.h -= 2;
      }
    }

    /* Quest tracker - top-left of viewport */
    QuestTrackerDraw( vp_rect );

    a_DisableClipRect();
  }

  /* Dialogue UI - drawn on top of viewport */
  if ( DialogueActive() )
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    aRectf_t cr = cp->rect;
    cr.y += TransitionGetConsoleOY();
    DialogueUIDraw( cr );
  }
  else if ( ShopUIActive() )
  {
    aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
    aRectf_t cr = cp->rect;
    cr.y += TransitionGetConsoleOY();
    ShopUIDraw( cr );
  }

  /* Tile action menu (drawn outside clip rect, over everything) */
  TileActionsDraw( enemies, num_enemies );

  if ( TransitionShowLabels() )
    a_DrawWidgets();

  InventoryUISetIntroOffset( TransitionGetRightOX(), TransitionGetUIAlpha() );
  InventoryUIDraw();

  /* Passive tooltip - drawn after viewport so it's on top */
  HUDDrawPassiveTooltip();

  /* First-consumable hint arrow */
  {
    float ht = GameTurnsHintTimer();
    if ( ht > 0.0f )
    {
    /* Compute alpha - fade out during last HINT_FADE seconds */
    float alpha = 1.0f;
    if ( ht < HINT_FADE )
      alpha = ht / HINT_FADE;

    /* Locate first inventory slot (replicate grid math from inventory_ui) */
    aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
    aRectf_t ir = ip->rect;
    ir.x += TransitionGetRightOX();

    float grid_y = ir.y + 30.0f;
    float grid_w = ir.w - 4.0f * 2;
    float grid_h = ir.h - 30.0f - 4.0f * 2;
    float cell_w = grid_w / INV_COLS;
    float cell_h = grid_h / INV_ROWS;
    float cell   = cell_w < cell_h ? cell_w : cell_h;
    float total_gw = cell * INV_COLS + 4 * ( INV_COLS - 1 );
    float total_gh = cell * INV_ROWS + 2 * ( INV_ROWS - 1 );
    float ox = ( grid_w - total_gw ) / 2.0f;
    float oy = ( grid_h - total_gh ) / 2.0f;

    float slot_x = ir.x + 4.0f + ox;
    float slot_y = grid_y + oy;
    float slot_cy = slot_y + ( cell - 1 ) / 2.0f;

    /* Bobbing animation */
    float bob = sinf( ( HINT_DURATION - ht ) * 4.0f ) * 4.0f;

    /* Arrow tip points at slot left edge */
    float tip_x = slot_x - 6.0f + bob;
    float tip_y = slot_cy;

    int a = (int)( 255 * alpha );
    aColor_t hint_color = { 0xcf, 0x57, 0x3c, a };

    /* Filled triangle arrowhead */
    a_DrawFilledTriangle(
      (int)tip_x, (int)tip_y,
      (int)( tip_x - 14 ), (int)( tip_y - 8 ),
      (int)( tip_x - 14 ), (int)( tip_y + 8 ),
      hint_color
    );

    /* Shaft */
    float shaft_x = tip_x - 14;
    float shaft_len = 30.0f;
    for ( int dy = -1; dy <= 1; dy++ )
      a_DrawLine( (int)( shaft_x - shaft_len ), (int)( tip_y + dy ),
                  (int)shaft_x, (int)( tip_y + dy ), hint_color );

    /* Text label */
    aTextStyle_t hts = a_default_text_style;
    hts.bg    = (aColor_t){ 0, 0, 0, 0 };
    hts.fg    = hint_color;
    hts.scale = 1.2f;
    hts.align = TEXT_ALIGN_RIGHT;
    a_DrawText( "USE CONSUMABLES IN TOUGH COMBATS!",
                (int)( shaft_x - shaft_len - 6 ), (int)( tip_y - 5 ), hts );
    }
  }

  /* Skip-turn hint text above console */
  {
    float st = GameTurnsSkipHintTimer();
    if ( st > 0.0f )
    {
      float alpha = 1.0f;
      if ( st < HINT_FADE ) alpha = st / HINT_FADE;

      aContainerWidget_t* cp = a_GetContainerFromWidget( "console_panel" );
      aRectf_t cr = cp->rect;
      cr.y += TransitionGetConsoleOY();

      aTextStyle_t sts = a_default_text_style;
      sts.bg    = (aColor_t){ 0, 0, 0, 0 };
      sts.fg    = (aColor_t){ 255, 255, 255, (int)( 255 * alpha ) };
      sts.scale = 2.0f;
      sts.align = TEXT_ALIGN_CENTER;
      a_DrawText( "Press SPACE to skip turn",
                  (int)( cr.x + cr.w / 2 ), (int)( cr.y - 48 ), sts );
    }
  }

  /* Inventory expansion hint arrow */
  {
    float et = GameTurnsInvExpandHintTimer();
    if ( et > 0.0f )
    {
      float alpha = 1.0f;
      if ( et < HINT_FADE ) alpha = et / HINT_FADE;

      aContainerWidget_t* ip = a_GetContainerFromWidget( "inv_panel" );
      aRectf_t ir = ip->rect;
      ir.x += TransitionGetRightOX();

      /* Point at the middle-left of the inventory panel */
      float tip_x = ir.x - 6.0f + sinf( ( HINT_DURATION - et ) * 4.0f ) * 4.0f;
      float tip_y = ir.y + ir.h * 0.5f;

      int a = (int)( 255 * alpha );
      aColor_t hc = { 0x75, 0xa7, 0x43, a };

      a_DrawFilledTriangle(
        (int)tip_x, (int)tip_y,
        (int)( tip_x - 14 ), (int)( tip_y - 8 ),
        (int)( tip_x - 14 ), (int)( tip_y + 8 ),
        hc
      );

      float shaft_x = tip_x - 14;
      for ( int dy = -1; dy <= 1; dy++ )
        a_DrawLine( (int)( shaft_x - 30 ), (int)( tip_y + dy ),
                    (int)shaft_x, (int)( tip_y + dy ), hc );

      aTextStyle_t ets = a_default_text_style;
      ets.bg    = (aColor_t){ 0, 0, 0, 0 };
      ets.fg    = hc;
      ets.scale = 1.2f;
      ets.align = TEXT_ALIGN_RIGHT;
      a_DrawText( "INVENTORY EXPANDED!",
                  (int)( shaft_x - 36 ), (int)( tip_y - 5 ), ets );
    }
  }

  /* Pause menu - drawn on top of everything */
  PauseMenuDraw();

  /* Dev mode overlay (teleport selector) */
  DevModeDraw();

  /* NPC relocate fade-to-black overlay */
  NPCRelocateDraw();

  /* Game over - drawn on top of absolutely everything */
  GameOverDraw();
}
