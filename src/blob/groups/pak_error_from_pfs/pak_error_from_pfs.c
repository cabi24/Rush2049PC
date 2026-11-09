/* pak_error_from_pfs: libultra's pak errors as the pak system's (MAT3 paksysdc.c). */
#include "game/game.h"

/* The pak system's error (kPS_Err*) for libultra's error code (PFS_ERR_*): no pak, another pak, inconsistent,
 * controller failure, invalid, both full errors, exists, bad data, and the unreadable ID or device; any other code
 * is kPS_ErrUnknown. */
s32 pak_error_from_pfs(s32 error) {
    switch (error) {
        case 0: return kPS_ErrNone;
        case 1: return kPS_ErrNoPak;          /* PFS_ERR_NOPACK */
        case 2: return kPS_ErrNewPak;         /* PFS_ERR_NEW_PACK */
        case 3: return kPS_ErrInconsistent;   /* PFS_ERR_INCONSISTENT */
        case 4: return kPS_ErrComm;           /* PFS_ERR_CONTRFAIL */
        case 5: return kPS_ErrFail;           /* PFS_ERR_INVALID */
        case 7: return kPS_ErrFull;           /* PFS_DATA_FULL */
        case 8: return kPS_ErrFull;           /* PFS_DIR_FULL */
        case 9: return kPS_ErrExist;          /* PFS_ERR_EXIST */
        case 6: return kPS_ErrBadData;        /* PFS_ERR_BAD_DATA */
        case 10:                              /* PFS_ERR_ID_FATAL */
        case 11: return kPS_ErrRecognize;     /* PFS_ERR_DEVICE */
        default: return kPS_ErrUnknown;
    }
}
