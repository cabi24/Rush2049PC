/* flags: -g0 -O2 -mips2 -G 0 -non_shared */
typedef signed char s8; typedef unsigned char u8; typedef short s16; typedef int s32; typedef float f32;
typedef struct { s32 a; s8 b; s8 c; u8 pad[6]; } SelectCarWave;
extern SelectCarWave gSelectCarWavePtrs[];
s32 WaveInstanceReady(s32 handle);
void UnloadWaveInstance(s32 handle);
void free_car_palette(s32 id);

void UnloadSelectCar(s16 id)
{
    s32 i;
    SelectCarWave *p;

    for (i = 0; i < 16; i++) {
        p = &gSelectCarWavePtrs[i];
        if (p->c == id) {
            while (WaveInstanceReady(p->a) == 0) {
            }
            UnloadWaveInstance(p->a);
            free_car_palette(id);
            p->a = -1;
            p->c = -1;
            p->b = 0;
            return;
        }
    }
}
