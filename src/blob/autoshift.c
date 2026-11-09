#include "game/game.h"

typedef struct Parameters {u8 prefix[172]; f32 upshift,downshift;} Parameters;
typedef struct Tuning {u8 prefix[20]; f32 shift_gain;} Tuning;
enum n64p_Parameters { n64p_Parameters_ };
enum n64p_Tuning { n64p_Tuning_ };
typedef struct Model2056 {
    enum n64p_Parameters parameters; enum n64p_Tuning tuning;
    u8 to_throttle[968]; f32 throttle;
    u8 to_gear[32]; s16 gear,commandgear;
    u8 to_engangvel[16]; f32 engangvel;
    u8 tail[1020];
} Model2056;
/* N64-address fields (tools/port/ptr64_u32fields.py) */
#define N64P(x) _Generic((x), enum n64p_Parameters: (Parameters *)PC_N64P(x), enum n64p_Tuning: (Tuning *)PC_N64P(x), default: (x))
#define N64W(x) PC_N64W(x)

void autoshift(Model2056 *m)
{
    f32 modupshiftangvel,moddownshiftangvel,fact;
    fact=(m->throttle+(f32)3.0)*(f32).25;
    modupshiftangvel=N64P(m->tuning)->shift_gain*N64P(m->parameters)->upshift*fact;
    moddownshiftangvel=N64P(m->tuning)->shift_gain*N64P(m->parameters)->downshift*fact;
    if (m->commandgear==0 || m->commandgear==-1) {
        m->gear=m->commandgear;
    } else {
        if (m->gear==0 || m->gear==-1)
            find_best_gear(m,modupshiftangvel,moddownshiftangvel);
        if (m->engangvel>modupshiftangvel) upshift(m);
        if (m->engangvel<moddownshiftangvel) downshift(m);
    }
}
