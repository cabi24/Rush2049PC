/* GENERATED ROM-aligned TU — segment 0x92f0 (rom/lib_92f0)
 * layout map 370de43169ac2fb4dabb5672cfad215eab737c20fc24fd05fdf6c50952f27393.
 * Slots are GLOBAL_ASM passthroughs until promoted; do not hand-edit
 * passthrough lines. */
#include "rom_tu.h"

/* PROMOTED 2026-09-24 — osSyncPrintf (ultralib name; formerly labelled osWritebackDCache_full)
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
void osSyncPrintf(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
  int new_var;
  if (!new_var)
  {
  }
  new_var = (int) (1 & 0xFF);
  if (new_var)
  {
  }
}

/* PROMOTED 2026-10-12 — rmonPrintf
 * Source:   reconstructed C, byte-matched against the retail ROM (2026-10)
 * Flags:    -g0 -O2 -mips2 -G 0 -non_shared
 * Evidence: identical bytes in the full-ROM SHA-1 build
 * Gate:     full-ROM SHA-1
 */
typedef char *va_list;
void rmonPrintf(const char* fmt, ...) {
    int ans;
    va_list ap;

}

