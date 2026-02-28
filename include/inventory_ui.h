#ifndef __INVENTORY_UI_H__
#define __INVENTORY_UI_H__

#include <Archimedes.h>

void InventoryUIInit( aSoundEffect_t* move, aSoundEffect_t* click );
int  InventoryUICloseMenus( void );
int  InventoryUILogic( int mouse_moved );
void InventoryUIDraw( void );
int  InventoryUIFocused( void );
void InventoryUIUnfocus( void );
void InventoryUISetIntroOffset( float x_offset, float alpha );

#endif
