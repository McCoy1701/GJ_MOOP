#include "dev_mode.h"

void DevModeInit( Console_t* console )  { (void)console; }
void DevModeSetNPCs( NPC_t* list, int* count ) { (void)list; (void)count; }
int  DevModeActive( void )  { return 0; }
int  DevModeInput( void )   { return 0; }
void DevModeDraw( void )    {}
int  DevModeNoclip( void )  { return 0; }
