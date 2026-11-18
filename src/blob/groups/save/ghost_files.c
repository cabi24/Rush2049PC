/* Ghost files: SaveGhost, reading, decoding and dropping a ghost's data, PS_PurgeFile (MAT3 save.c and ghost.c). */
#include "game/game.h"
#define OSMesgQueue OSMesgQueueView   /* this file's own OSMesgQueue: include/game's OSMesgQueue is another type */

typedef struct OSMesgQueue OSMesgQueue;
s32 osRecvMesg(OSMesgQueue *,void **,s32);s32 osSendMesg(OSMesgQueue *,void *,s32);

s32 sprintf(char *, const char *, ...);
void *memcpy(void *, const void *, u32);

/* A ghost's decoded recording is its data block (datah); its header (tGhostHeader) says it has it (hasSamples: MAT3
 * kept the block's address there, as start). */
s32 unzipdata(void *, s32, void *);

/* Frees an open pak file's data (it is read again when needed) and clears the file's lock flag. */
void PS_PurgeFile(tPakHandle *fileh) {
    PakFileRecord *s = fileh->file;

    if (s->data != NULL) {
        u8 **data = s->data;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(data, 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        s->data = NULL;
        gPaks[s->pak].files[s->fileNo].flag2 = 0;
    }
}

/* Frees a ghost's decoded recording and its file's data (PS_PurgeFile); its header no longer has its samples. */
void PurgeGhostData(tGhostFile **h) {
    tGhostFile *o = *h;

    if (o->datah != NULL) {
        u8 **old = o->datah;
        osRecvMesg(&heap_lock_queue, NULL, 1);
        DeallocateMemoryInternal(old, 0);
        osSendMesg(&heap_lock_queue, NULL, 0);
        o->datah = NULL;
        (*o->header)->hasSamples = 0;
        if (o->fileh != NULL) {
            PS_PurgeFile(o->fileh);
            o->header = NULL;
        }
    }
}

/* Decompresses a ghost's recording (unzipdata) into a new heap block, the ghost's data (datah), once: the header
 * then has its samples, their number (three bytes each) and is to be played back (flag 1). */
void DecodeGhost(tGhostFile **h) {
    tGhostHeader *m = *(*h)->header;
    void *buffer;
    s32 n;

    if (m->hasSamples == 0) {
        (*h)->datah = AllocateHandleHeap(NULL, m->unzippedSize);
        buffer = *(*h)->datah;
        m->hasSamples = 1;
        n = unzipdata(m + 1, m->zippedSize, buffer);
        m->size = n / 3;
        do {} while (0);
        CalcChecksumInternal(buffer, n);
        m->flag = 1;
    }
}

/* The heap lock (MAT3's DeallocateMemory, inline in SaveGhost: take, free, give back). */
void ghost_heap_lock(void) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
}
/* Gives the heap lock back. */
void ghost_heap_unlock(void) {
    osSendMesg(&heap_lock_queue, NULL, 0);
}
/* Frees the heap block at address, under the heap lock. */
void cart_free(void **address) {
    ghost_heap_lock();
    DeallocateMemoryInternal(address, 0);
    ghost_heap_unlock();
}

/* Forgets the ghost node if a player races against it (gGhosts). */
void slot_forget(tGhostFile **node) {
    s32 i;
    for (i = 0; i < 4; i++) {
        if (gGhosts[i] == node) {
            gGhosts[i] = NULL;
            break;
        }
    }
}

/* Frees the heap block at address, under the heap lock (cart_free written out). */
void cart_free_direct(void **address) {
    osRecvMesg(&heap_lock_queue, NULL, 1);
    DeallocateMemoryInternal(address, 0);
    osSendMesg(&heap_lock_queue, NULL, 0);
}
/* Deletes the ghost's file from its pak, if it has one. */
void DeleteGhost(tGhostFile **ghosth) {
    if ((*ghosth)->fileh != NULL) {
        PS_DeleteFile((*ghosth)->fileh);
    }
}

/* The ghost file name's direction suffix: backward or forward, by the header. */
char *GhostDirName(tGhostHeader *header) {
    if (header->backward) {
        return ghost_backward_suffix;
    }
    return ghost_forward_suffix;
}

/* Saves the new ghost saveGhosth of profile player on pak's pak (MAT3 SaveGhost(pak, saveGhosth, limbo); the N64 adds
 * the player). With limbo it is only kept in memory (SaveGhostInLimbo) and freed. Else an older ghost of the same
 * driver, track and direction on that pak goes to limbo and is deleted; a file named after the player's file, the
 * track and the direction is created, the ghost written to it and its data dropped from memory, and the ghost joins
 * the ghost list in order of its time. Returns 0 when the file can't be created. */
s32 SaveGhost(s32 pak, tProfile **player, tGhostFile **saveGhosth, s32 limbo) {
    tGhostHeader *m;
    tGhostFile **node;
    char buf[32];
    tGhostFile *obj;
    tPakHandle *created;
    u32 size;

    m = *(*saveGhosth)->header;
    if (limbo) {
        SaveGhostInLimbo(m, m + 1);
        cart_free_direct((void **)(*saveGhosth)->header);
        FreeGhost(saveGhosth);
        return 1;
    }
    for (node = gGhostFiles.head; node != NULL; node = obj->next) {
        obj = *node;
        if (pak == obj->fileh->file->pak && game_strcmp(obj->name, m->name) == 0 &&
            m->playerId[0] == obj->playerId[0] && m->playerId[1] == obj->playerId[1] &&
            obj->track == m->track && obj->backward == m->backward) {
            slot_forget(node);
            if (ReadGhostDataInternal(node, 0, 0)) {
                SaveGhostInLimbo(*obj->header, *obj->header + 1);
                DeleteGhost(node);
            }
            break;
        }
    }
    obj = *saveGhosth;
    size = GetPtrSize(obj->header);
    sprintf(buf, ghost_file_name_format, (*player)->fileh->file->name, m->track + 1, GhostDirName(m));
    created = PS_CreateFile(pak, buf, ghost_file_ext, size, GAME_COMPANY_CODE, GAME_CODE);
    if (created == NULL) {
        return 0;
    }
    obj->fileh = created;
    created->file->RemoveFile = RemovePlayerFile;
    obj->fileh->file->MoveFile = MovePlayerFile;
    memcpy(*created->file->data, *obj->header, size);
    cart_free((void **)obj->header);
    obj->header = (tGhostHeader **)created->file->data;
    PS_WriteFile(created, *obj->header, size);
    PS_FlushFile(created, 0);
    PS_FlushPak(pak, 1);
    PS_PurgeFile(created);
    obj->header = NULL;
    obj->datah = NULL;
    for (node = gGhostFiles.head; node != NULL; node = (*node)->next) {
        if (obj->finish < (*node)->finish) {
            break;
        }
    }
    InsertLLE(&gGhostFiles, saveGhosth, node);
    return 1;
}

/* Reads the ghost's file, checks it, decodes its recording and moves both low in the heap (ReadGhostDataInternal). */
s32 ReadGhostData(tGhostFile **h) {
    return ReadGhostDataInternal(h, 1, 1);
}
