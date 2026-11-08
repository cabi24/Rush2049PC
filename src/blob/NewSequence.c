/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef unsigned char u8;typedef unsigned int u32;
/* PC: a sequence (audiomgr_thread.c's anon_ll_id_3c) has host-sized links (docs/port/SYNTH.md). */
typedef struct Node16 {void *pad0[2];u32 value;u8 busy,pad13[3];} Node16;
typedef struct List16 {u8 indirect,doubly,pad2[2];u32 count;Node16 *head,*tail;} List16;
extern List16 gFreeSequences;extern u32 gNextSeqId,gTagShift,gTagMask;
void RemoveLLE(List16 *,Node16 *);
Node16 *NewSequence(void) {
 Node16 *node=gFreeSequences.head;
 RemoveLLE(&gFreeSequences,node);
 node->busy=0;
 node->value=(gNextSeqId<<gTagShift)|(node->value&gTagMask);
 if(gNextSeqId>=((~gTagMask)>>(gTagShift+1)))gNextSeqId=1;
 else gNextSeqId++;
 return node;
}
