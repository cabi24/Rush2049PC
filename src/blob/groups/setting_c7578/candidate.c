typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u32;
#define M2C_FIELD(expr, type, offset) (*(type)((u8 *)(expr) + (offset)))
#ifdef TARGET_PC
#define SLOT_DATA PROFILE_OFS(0x2C)  /* SaveSlot.data: the profile record's PC layout (pc_prelude.h) */
#else
#define SLOT_DATA 44
#define PROFILE_OFS(n) n
#endif
extern u32 format_string_parse(u8 *, u32);
extern void PS_WriteFile(void *, void *, u32);
typedef struct Setting { u32 checksum; s8 fields[12]; } Setting;
typedef struct SaveData { u8 prior[1856]; Setting settings[13]; } SaveData;
void func_800C7578(void *obj,u8 index,u8 field,s8 value) {
 SaveData *base=M2C_FIELD(M2C_FIELD(M2C_FIELD(obj,void **,0),void **,SLOT_DATA),SaveData **,0);
 if(base->settings[index].fields[field] != value) {
  base->settings[index].fields[field]=value;
  base->settings[index].checksum=format_string_parse((u8 *)base->settings[index].fields,12);
  PS_WriteFile(M2C_FIELD(M2C_FIELD(obj,void **,0),void **,PROFILE_OFS(8)),&base->settings[index],16);
 }
}
