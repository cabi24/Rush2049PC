#include "game/game.h"

/* PC: the pak file record's host layout (pc_prelude.h, docs/port/POINTERS.md 9.10) */
typedef struct Object { void *next, *prev; void (*RemoveFile)(void *), (*MoveFile)(void *, void *); u8 port, file; } Object;
typedef struct Handle { Object *object; } Handle;
typedef struct PakFile40 { s8 active; u8 opaque1[39]; } PakFile40;

typedef struct Pak772 {
    u8 index;
    s8 enabled;
    u8 opaque2[7];
    s8 retained;
    s8 files_active;
    u8 opaque11;
    PcPfs pfs;
    s32 error, FreeSpace;
    u8 opaque7C[8];
    PakFile40 files[16];
} Pak772;

VIEW(volatile Pak772[], gPaks);
#define gPaks gPaks_view

void PS_LockFile(Handle *handle, s32 active)
{
    s32 port;
    s32 i;

    if (handle != 0) {
        port = handle->object->port;
        if (active != gPaks[port].files[handle->object->file].active) {
            gPaks[port].files[handle->object->file].active = active;
            if (active) {
                gPaks[port].files_active = 1;
            } else {
                for (i = 0; i < 16; i++) {
                    if (gPaks[port].files[i].active) {
                        break;
                    }
                }
                if (i >= 16) {
                    gPaks[port].files_active = 0;
                    if (gPaks[port].retained == 0) {
                        gPaks[port].enabled = 0;
                    }
                }
            }
        }
    }
}
