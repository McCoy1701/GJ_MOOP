#include <stdio.h>
#include <Archimedes.h>

#include "dialogue.h"
#include "dungeon.h"

#define QT_PAD_X   8.0f
#define QT_PAD_Y   6.0f
#define QT_SCALE   1.0f

#define QT_YELLOW  (aColor_t){ 0xde, 0x9e, 0x41, 255 }
#define QT_GREEN   (aColor_t){ 0x75, 0xa7, 0x43, 255 }

void QuestTrackerDraw( aRectf_t vp_rect )
{
  aTextStyle_t ts = a_default_text_style;
  ts.bg    = (aColor_t){ 0, 0, 0, 0 };
  ts.scale = QT_SCALE;
  ts.align = TEXT_ALIGN_LEFT;

  float x = vp_rect.x + QT_PAD_X;
  float y = vp_rect.y + QT_PAD_Y;

  char buf[128];

  /* Help Graf find a way out */
  if ( FlagGet( "quest_help_graf" ) && !FlagGet( "got_sandwich" ) )
  {
    if ( FlagGet( "has_mayors_note" ) )
    {
      snprintf( buf, sizeof( buf ), "Give Graf the Mayor's Note" );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Help Graf find a way out" );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float twg, thg;
    a_CalcTextDimensions( buf, ts.type, &twg, &thg );
    y += thg * QT_SCALE + QT_PAD_Y;
  }

  /* Eat the sandwich */
  if ( FlagGet( "got_sandwich" ) && !FlagGet( "sandwich_eaten" ) )
  {
    snprintf( buf, sizeof( buf ), "Eat Graf's sandwich" );
    ts.fg = QT_GREEN;

    a_DrawText( buf, (int)x, (int)y, ts );

    float tws, ths;
    a_CalcTextDimensions( buf, ts.type, &tws, &ths );
    y += ths * QT_SCALE + QT_PAD_Y;
  }

  /* Find the next floor */
  if ( g_current_floor == 1 && FlagGet( "sandwich_eaten" ) )
  {
    snprintf( buf, sizeof( buf ), "The dungeon goes deeper..." );
    ts.fg = QT_YELLOW;

    a_DrawText( buf, (int)x, (int)y, ts );

    float twd, thd;
    a_CalcTextDimensions( buf, ts.type, &twd, &thd );
    y += thd * QT_SCALE + QT_PAD_Y;
  }

  /* Rat quest */
  if ( FlagGet( "quest_rats" ) )
  {
    int kills = FlagGet( "rat_kills" );
    if ( kills > 3 ) kills = 3;

    if ( kills >= 3 )
    {
      snprintf( buf, sizeof( buf ), "Rats killed: %d/3 - Return to Graf", kills );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Rats killed: %d/3", kills );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw, th;
    a_CalcTextDimensions( buf, ts.type, &tw, &th );
    y += th * QT_SCALE + QT_PAD_Y;
  }

  /* Skeleton quest */
  if ( FlagGet( "quest_skeletons" ) )
  {
    int kills = FlagGet( "skeleton_kills" );
    if ( kills > 3 ) kills = 3;

    if ( kills >= 3 )
    {
      snprintf( buf, sizeof( buf ), "Skeletons killed: %d/3 - Return to Jonathon", kills );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Skeletons killed: %d/3", kills );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw2, th2;
    a_CalcTextDimensions( buf, ts.type, &tw2, &th2 );
    y += th2 * QT_SCALE + QT_PAD_Y;
  }

  /* Relic quest */
  if ( FlagGet( "quest_relic" ) && !FlagGet( "relic_returned" ) )
  {
    if ( FlagGet( "has_relic" ) )
    {
      snprintf( buf, sizeof( buf ), "Return Hearthstone to Thistlewick" );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Retrieve the Goblin Hearthstone" );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw3, th3;
    a_CalcTextDimensions( buf, ts.type, &tw3, &th3 );
    y += th3 * QT_SCALE + QT_PAD_Y;
  }

  /* Mushroom quest */
  if ( FlagGet( "quest_mushrooms" ) )
  {
    int m = FlagGet( "mushrooms_collected" );
    if ( m > 3 ) m = 3;

    if ( m >= 3 )
    {
      snprintf( buf, sizeof( buf ), "Mushrooms: %d/3 - Deliver mushrooms", m );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Mushrooms: %d/3", m );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw4, th4;
    a_CalcTextDimensions( buf, ts.type, &tw4, &th4 );
    y += th4 * QT_SCALE + QT_PAD_Y;
  }

  /* Red slime quest */
  if ( FlagGet( "quest_slimes" ) && !FlagGet( "quest_slimes_done" ) )
  {
    int kills = FlagGet( "red_slime_kills" );
    if ( kills > 3 ) kills = 3;

    if ( kills >= 3 )
    {
      snprintf( buf, sizeof( buf ), "Red slimes killed: %d/3 - Return to Glorbnax", kills );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Red slimes killed: %d/3", kills );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw5, th5;
    a_CalcTextDimensions( buf, ts.type, &tw5, &th5 );
    y += th5 * QT_SCALE + QT_PAD_Y;
  }

  /* Spider quest */
  if ( FlagGet( "quest_spiders" ) && !FlagGet( "quest_spiders_done" ) )
  {
    int kills = FlagGet( "spider_kills" );
    if ( kills > 3 ) kills = 3;

    if ( kills >= 3 )
    {
      snprintf( buf, sizeof( buf ), "Spiders killed: %d/3 - Return to Burble", kills );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Spiders killed: %d/3", kills );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw6, th6;
    a_CalcTextDimensions( buf, ts.type, &tw6, &th6 );
    y += th6 * QT_SCALE + QT_PAD_Y;
  }

  /* Meet Laura at south tunnel */
  if ( FlagGet( "laura_relocated" ) && !FlagGet( "quest_return_laura" ) )
  {
    snprintf( buf, sizeof( buf ), "Meet Laura in the south tunnel" );
    ts.fg = QT_GREEN;

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw7, th7;
    a_CalcTextDimensions( buf, ts.type, &tw7, &th7 );
    y += th7 * QT_SCALE + QT_PAD_Y;
  }

  /* Return to Laura */
  if ( FlagGet( "quest_return_laura" ) )
  {
    snprintf( buf, sizeof( buf ), "Return to Laura" );
    ts.fg = QT_GREEN;

    a_DrawText( buf, (int)x, (int)y, ts );

    float tw8, th8;
    a_CalcTextDimensions( buf, ts.type, &tw8, &th8 );
    y += th8 * QT_SCALE + QT_PAD_Y;
  }

  /* Find Bloop */
  if ( FlagGet( "quest_bloop" ) && !FlagGet( "told_drem_rescued" ) )
  {
    if ( FlagGet( "found_horror_rescued" ) )
    {
      snprintf( buf, sizeof( buf ), "Return to Drem" );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Find Bloop" );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );
  }
}
