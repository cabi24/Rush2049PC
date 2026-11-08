/* port/ultra/gsm.h: the GSM 06.10 decoder state as the game lays it out (gsm.c's GsmState), and the
 * three kernels port/ultra/gsm.c provides for gsm.c's decoder. */
#ifndef PORT_ULTRA_GSM_H
#define PORT_ULTRA_GSM_H

/* gsm.c's layout (0x190 bytes). The kernels own dp0:
 *   [0, 160)    0x000  the reconstructed short-term residual drp, oldest first; the newest 40 are drp[0..39].
 *                      After a frame's four subframes, [0, 160) is the short-term filter's input wt.
 *   [160, 176)  0x140  the short-term filter state v[0..7], as eight 32-bit words
 *   [178]       0x164  msr, the deemphasis filter state
 *   [179]       0x166  nrp, the last valid LTP lag (40 at stream start: speech_reset_decoder's block_count)
 * The stream setup bzeroes the rest. */
typedef struct GsmState {
    short dp0[180];
    short LARpp[2][8];
    short j;
    short repeat;
    short silence;
    unsigned char unknown18E[2];
} GsmState;

enum { GSM_V_OFFSET = 0x140, GSM_MSR = 0x164 / 2, GSM_NRP = 0x166 / 2 };

void gsm_rpe_decode(short xmaxcr, short Mcr, short *xMcr, short *erp);   /* RPE decoding */
void gsm_long_term_synthesis(GsmState *S, short Ncr, short bcr, short *erp); /* long-term synthesis */
void gsm_short_term_filter(GsmState *S, short *wt, short *s);             /* short-term synthesis, postprocessing */

#endif
