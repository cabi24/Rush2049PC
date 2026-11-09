/* InitSTree: finds the parts of the track's collision WAD (MAT3 stree.c). */
#include "game/game.h"

/* Sets the surface tree's pointers into the loaded collision WAD (gRoadDat): after its header (stheadertyp) come
 * the road path (gRoadPath: 132-byte points, MAT3 pathtyp), the tree's nodes (stree_nodes: 20 bytes, streetyp),
 * the surfaces (stree_surf: 24 bytes, surftyp), the vertices (stree_svert: 8 bytes, sverttyp), the saved surfaces
 * of the collision objects (stree_save: 32 bytes, savetyp), then the vertex and surface lists. The data is already
 * in host byte order (MAT3 swaps it here). */
void InitSTree(void) {
    stheadertyp *hp = gRoadDat;
    gRoadHead = hp;
    gRoadPath = (struct pathtyp *)(hp + 1);
    stree_nodes = (streetyp *)((u8 *)gRoadPath + hp->pnum * 132);
    stree_surf = (surftyp *)(stree_nodes + hp->tnum);
    stree_svert = (sverttyp *)(stree_surf + hp->snum);
    stree_save = (savetyp *)(stree_svert + hp->vnum);
    stree_savenum = hp->savenum;
    stree_vlist = (u8 *)(stree_save + hp->savenum);
    stree_slist = stree_vlist + hp->vlnum;
}
