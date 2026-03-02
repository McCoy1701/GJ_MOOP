#include <stdio.h>
#include <Archimedes.h>

#include "dialogue.h"

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
      snprintf( buf, sizeof( buf ), "Mushrooms: %d/3 - Return to Nettle", m );
      ts.fg = QT_GREEN;
    }
    else
    {
      snprintf( buf, sizeof( buf ), "Mushrooms: %d/3", m );
      ts.fg = QT_YELLOW;
    }

    a_DrawText( buf, (int)x, (int)y, ts );
  }
}
