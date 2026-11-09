/* displaylist_set_tile_size.c: change the tile rectangle a texture's load display list sets.
 *
 * TextureAnimation's texture scrolls move a texture by rewriting the corner of its G_SETTILESIZE commands. */
#include "game/game.h"

/* A display list command: two big-endian words. */
typedef struct Command {u32 w0,w1;} Command;
/* PC: the WAD's TXLD lists stay in N64 form, 8-byte big-endian commands (port/assets/wad.c). */
#define DLW(w) pc_be32_get(&(w))
#define DLW_SET(w, v) pc_be32_set(&(w), v)

/* The F3DEX2 commands looked for, in a command's top byte. */
#define GBI_SETTILESIZE 0xF2
#define GBI_ENDDL       0xDF

/* In every G_SETTILESIZE of the display list `commands` up to its G_ENDDL, set the tile's corners: uls and ult the
 * upper left (bits 12-23 and 0-11 of the first word), lrs and lrt the lower right (of the second word), in the
 * RDP's quarter texels; a negative value leaves its field as it is. The first tile's size (lrs + 4, lrt + 4: its
 * width and height in quarter texels) is the reference: for each later, smaller tile (the smaller levels of a
 * mipmap) uls and ult are halved as its size is. */
void displaylist_set_tile_size(Command *commands,s32 uls,s32 ult,s32 lrs,s32 lrt) {
    s32 width=-1,height=-1,size;
    u8 opcode;
    for(;;commands++){
        opcode=(DLW(commands->w0)&0xff000000)>>24;
        if((opcode&0xc0)==0x40 || (opcode&0xc0)==0x80 || (opcode>=9 && opcode<64) || (opcode>=192 && opcode<214))continue;
        if(opcode==GBI_SETTILESIZE){
            if(uls>=0){
                size=((DLW(commands->w1)&0x00fff000)>>12)+4;
                if(width<0)width=size;
                else while(size<width){width >>= 1;
                    uls >>= 1;}
                DLW_SET(commands->w0, (DLW(commands->w0)&0xff000fff)|(uls<<12));
            }
            if(ult>=0){
                size=(DLW(commands->w1)&0xfff)+4;
                if(height<0)height=size;
                else while(size<height){height >>= 1;
                    ult >>= 1;}
                DLW_SET(commands->w0, (DLW(commands->w0)&0xfffff000)|ult);
            }
            if(lrs>=0)DLW_SET(commands->w1, (DLW(commands->w1)&0xff000fff)|(lrs<<12));
            if(lrt>=0)DLW_SET(commands->w1, (DLW(commands->w1)&0xfffff000)|lrt);
        }else if(opcode==GBI_ENDDL)break;
    }
}
