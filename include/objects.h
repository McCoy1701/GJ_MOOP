#ifndef __OBJECTS_H__
#define __OBJECTS_H__

#include "world.h"
#include "console.h"

#define OBJ_EASEL  100
#define OBJ_CHAIR  101

void ObjectsInit( Console_t* con );
void ObjectPlace( World_t* w, int x, int y, int type );
int  ObjectIsObject( int x, int y );
void ObjectDescribe( int x, int y );

#endif
