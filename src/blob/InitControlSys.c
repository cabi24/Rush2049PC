/* flags: -g0 -O3 -mips2 -G 0 -non_shared */
/*
 * InitControlSys (0x800E7038, 252 B), : controller-subsystem (re)initialisation, N64-only (no arcade
 * or MAT3 GameCube ancestor). Called by ControlIRQ (the pad reader) while gInitControls is clear and by
 * InitRushControls. The first time it enables every controller (SetControllerActive(-1, 1), the per-controller connected
 * flags) and turns stick-as-dpad on for all (player_mode_set(-1, 1)); it then resets the per-axis stick
 * calibration of the four controllers (gStickCal, gContStickRange) to 70, creates the SI lock queue controlsys_lock_queue
 * (one message, controlsys_lock_msg) once and primes it, waits while the osContInit bit-pattern byte cont_bit_pattern reads 0x80,
 * and reads the pads (UpdateControls).
 * Shaping (each verified necessary):
 *  - the queue's created-once flag is a FUNCTION-LOCAL static (own .data 0x80111968; no other function references
 *    that address). As a global, uopt keeps its address and the shared constant 1 of both flag stores in
 *    registers (v0/v1, 46 words); as a local static the stores take a fresh `li t7,1` each, as retail does;
 *  - the calibration reset is a 4-trip loop that IDO unrolls completely (8 separate `li tN,70`; straight-line
 *    stores share one 70 register), with [1] stored before [0] (ugen temp ring t8,t9,t0..t5 as retail);
 *  - gStickCal is DEFINED here, so as1 shares one `lui at` over the 8 stores (with an extern each store gets its
 *    own lui);
 *  - the two calibration stores are on ONE line: on two lines as1 swaps the last pair of stores (2 words).
 */
typedef signed char s8;
typedef unsigned char u8;
typedef signed int s32;
typedef void *OSMesg;
typedef struct OSThread OSThread;
typedef struct OSMesgQueue {OSThread *mtqueue,*fullqueue;s32 validCount,first,msgCount;OSMesg *msg;} OSMesgQueue;

extern s8 gInitControls;              /* controller subsystem initialised (also read by ControlIRQ) */
extern u8 cont_bit_pattern;              /* osContInit bit pattern */
extern s8 gContStickRange[4][2];               /* gStickCal: per-controller x/y stick calibration */
extern OSMesgQueue controlsys_lock_queue;     /* SI lock queue */
extern OSMesg controlsys_lock_msg;          /* its one-message buffer */
void SetControllerActive(s32 player, s32 value);
void SetControllerStickEdges(s32 player, s32 value);
void UpdateControls(void);
extern void osCreateMesgQueue(OSMesgQueue *, OSMesg *, s32);
extern s32 osSendMesg(OSMesgQueue *, OSMesg, s32);

void InitControlSys(void)
{
    static s8 queueCreated = 0;    /* 0x80111968 */
    s32 i;

    if (gInitControls == 0) {
        SetControllerActive(-1, 1);
        SetControllerStickEdges(-1, 1);
    }
    gInitControls = 1;
    for (i = 0; i < 4; i++) {
        gContStickRange[i][1] = 70; gContStickRange[i][0] = 70;
    }
    if (queueCreated == 0) {
        queueCreated = 1;
        osCreateMesgQueue(&controlsys_lock_queue, &controlsys_lock_msg, 1);
        osSendMesg(&controlsys_lock_queue, 0, 0);
    }
    while (*(volatile u8 *)&cont_bit_pattern == 0x80) { pc_os_spin_yield("InitControlSys");
    }
    UpdateControls();
}
