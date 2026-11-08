/* flags: -g0 -O2 -mips2 -G 0 -non_shared -Wab,-r4300_mul */
typedef unsigned short u16;
typedef unsigned char u8;
typedef struct Header {u16 count[6]; u8 reserved[4];} Header;
extern Header *gRoadDat;
extern Header *gRoadHead;
extern u8 *gRoadPath;
extern u8 *stree_nodes;
extern u8 *stree_surf;
extern u8 *stree_svert;
extern u8 *stree_save;
extern u16 stree_savenum;
extern u8 *stree_vlist;
extern u8 *stree_slist;
void InitSTree(void) {
    Header *header = gRoadDat;
    gRoadHead = header;
    gRoadPath = (u8 *)(header + 1);
    stree_nodes = gRoadPath + header->count[0] * 132;
    stree_surf = stree_nodes + header->count[1] * 20;
    stree_svert = stree_surf + header->count[2] * 24;
    stree_save = stree_svert + header->count[3] * 8;
    stree_savenum = header->count[4];
    stree_vlist = stree_save + header->count[4] * 32;
    stree_slist = stree_vlist + header->count[5];
}
