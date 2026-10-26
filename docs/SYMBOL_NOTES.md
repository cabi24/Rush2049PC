# Symbol notes: static names corrected against ultralib

The static code's first names came from early analysis and from splat's libultra symbol list, and many of them
belonged to a different libultra function. Every name below was corrected against decompals' ultralib
(`e24c836`, `VERSION_J`, `-D_FINALROM`), which this ROM's libultra matches. The "former label" column is what
older notes, symbol dumps and tools may still call the function. Ultralib file paths are relative to its `src/`.

Evidence, by table:

* **Inventory names.** Each row's body was compared with the named ultralib function; the names formed cycles
  (`osSendMesg`/`osJamMesg`, `__osPiRawStartDma`/`osPiStartDma`, ...) and were applied as one permutation.
* **64-bit helpers, timer services, contpfs/timerintr data, sptask/controller/siacs/pfsgetstatus, exceptasm/TLB:**
  proof by compilation or assembly. The ultralib file was built with this project's IDO 5.3 at the holding TU's
  flags (or ultralib's own recipe for `.s` files), and each object's whole `.text` equals the ROM words, with
  relocations compared by value.
* **Leads:** structural identity (call order, field offsets); `osSyncPrintf` / `rmonPrintf` are identified by
  position (both are `_FINALROM` empty varargs bodies; segment `lib_92f0` holds exactly the two functions of
  `syncprintf.o`).

## libultra inventory names

| VRAM | Former label | ultralib name | ultralib source |
|------|--------------------|----------------|-----------------|
| 0x80006A30 | `osSetEventMesg` | `osCreateViManager` | `src/io/vimgr.c` |
| 0x80006D40 | `osSetThreadPri` | `osViSetMode` | `src/io/visetmode.c` |
| 0x80006DA0 | `osSetIntMask` | `osViBlack` | `src/io/viblack.c` |
| 0x80006E10 | `osSetEventMesgAlt` | `osSetEventMesg` | `src/os/seteventmesg.c` |
| 0x80006ED0 | `osSetTimerIntr` | `osViSetEvent` | `src/io/visetevent.c` |
| 0x800071D0 | `osSetGlobalIntMask` | `osSetIntMask` | `src/os/setintmask.s` |
| 0x80007440 | `osSendMesg` | `osJamMesg` | `src/os/jammesg.c` |
| 0x80007590 | `osViSetMode` | `osViSwapBuffer` | `src/io/viswapbuf.c` |
| 0x800075E0 | `osJamMesg` | `osSendMesg` | `src/os/sendmesg.c` |
| 0x800077D0 | `osViGetFramebuffer` | `osViGetNextFramebuffer` | `src/io/vigetnextframebuf.c` |
| 0x80007810 | `osInvalICache` | `osWritebackDCacheAll` | `src/os/writebackdcacheall.s` |
| 0x8000794C | `osViModeNtscLan1` | `osSpTaskLoad` | `src/io/sptask.c` |
| 0x80007AB4 | `osViModeNtscLpn1` | `osSpTaskStartGo` | `src/io/sptask.c` |
| 0x80007B80 | `osDpWait` | `osSpTaskYield` | `src/io/sptaskyield.c` |
| 0x800081D0 | `osPiRawReadWord` | `osPiReadIo` | `src/io/piread.c` |
| 0x80008380 | `osCreateViManager` | `osSetThreadPri` | `src/os/setthreadpri.c` |
| 0x80008460 | `osInvalICache_full` | `osInvalICache` | `src/os/invalicache.s` |
| 0x80008630 | `__osPiRawStartDma` | `osPiStartDma` | `src/io/pidma.c` |
| 0x8000870C | `osWritebackDCacheAll` | `rmonPrintf` | `src/libc/syncprintf.c` |
| 0x80009450 | `__osContBuildPacket` | `osContInit` | `src/io/controller.c` |
| 0x800097D0 | `osContStartReadData2` | `osContStartReadData` | `src/io/contreaddata.c` |
| 0x80009F20 | `osMotorInit` | `__osMotorAccess` | `src/io/motor.c` |
| 0x8000A088 | `__osMotorAccess` | `__osMakeMotorData` | `src/io/motor.c` |
| 0x8000A194 | `osMotorStart` | `osMotorInit` | `src/io/motor.c` |
| 0x8000A2F0 | `osMotorStop` | `osPfsRepairId` | `src/io/pfsrepairid.c` |
| 0x8000A350 | `osPfsAllocate` | `osPfsFindFile` | `src/io/pfssearchfile.c` |
| 0x8000A520 | `osPfsDeleteFile` | `osPfsFileState` | `src/io/pfsfilestate.c` |
| 0x8000A700 | `osPfsRename` | `osPfsDeleteFile` | `src/io/pfsdeletefile.c` |
| 0x8000A8D8 | `osPfsFindFile` | `__osPfsReleasePages` | `src/io/pfsdeletefile.c` |
| 0x8000A970 | `osPfsReadWriteFile` | `osPfsAllocateFile` | `src/io/pfsallocatefile.c` |
| 0x8000ADF0 | `osPfsGetFileStat` | `__osPfsGetNextPage` | `src/io/pfsreadwritefile.c` |
| 0x8000AEC4 | `osPfsGetFileSize` | `osPfsReadWriteFile` | `src/io/pfsreadwritefile.c` |
| 0x8000BD90 | `osPfsChecker_full` | `osStopThread` | `src/os/stopthread.c` |
| 0x8000BE50 | `osPfsReAllocate` | `osGetThreadId` | `src/os/getthreadid.c` |
| 0x8000C490 | `dll_get_priority` | `osGetThreadPri` | `src/os/getthreadpri.c` |
| 0x8000D7A0 | `osPiReadIo` | `__osSiRawReadIo` | `src/io/sirawread.c` |
| 0x8000D7F0 | `osPiWriteWord` | `__osSiRawWriteIo` | `src/io/sirawwrite.c` |
| 0x8000DC10 | `osPiGetAccess` | `__osPiGetAccess` | `src/io/piacs.c` |
| 0x8000DC54 | `osPiReleaseAccess` | `__osPiRelAccess` | `src/io/piacs.c` |
| 0x8000DC80 | `osPiReadWord` | `__osPiRawReadIo` | `src/io/pirawread.c` |
| 0x8000DCD0 | `osPiStartDma` | `__osPiRawStartDma` | `src/io/pirawdma.c` |
| 0x8000DDA0 | `osPiSetDeviceTiming` | `__osEPiRawStartDma` | `src/io/epirawdma.c` |
| 0x8000E620 | `osContStartReadData` | `__osPfsGetStatus` | `src/io/pfsgetstatus.c` |

## `libc/ll.c` and `llcvt.c` (64-bit helpers; formerly libgcc names)

Compiled `-g0 -O1 -mips3 -32`. IDO lowers 64-bit arithmetic in C to calls of exactly these names.

| vram | former label | ultralib name | file |
|---|---|---|---|
| 0x8000D8F0 | `__lshrdi3` | `__ull_rshift` | ll.c |
| 0x8000D91C | `__umoddi3` | `__ull_rem` | ll.c |
| 0x8000D958 | `__udivdi3` | `__ull_div` | ll.c |
| 0x8000D994 | `__ashldi3` | `__ll_lshift` | ll.c |
| 0x8000D9C0 | `__umoddi3_alt` | `__ll_rem` | ll.c |
| 0x8000D9FC | `__divdi3` | `__ll_div` | ll.c |
| 0x8000DA58 | `__muldi3` | `__ll_mul` | ll.c |
| 0x8000DA88 | `__qdivrem` | `__ull_divremi` | ll.c |
| 0x8000DAE8 | `__moddi3` | `__ll_mod` | ll.c |
| 0x8000DB84 | `__ashrdi3` | `__ll_rshift` | ll.c |
| 0x800099F0 | `__fixdfdi` | `__d_to_ll` | llcvt.c |
| 0x80009A0C | `__fixsfdi` | `__f_to_ll` | llcvt.c |
| 0x80009A28 | `__fixunsdfdi` | `__d_to_ull` | llcvt.c |
| 0x80009AC8 | `__fixunssfdi` | `__f_to_ull` | llcvt.c |
| 0x80009B64 | `__floatdidf` | `__ll_to_d` | llcvt.c |
| 0x80009B7C | `__floatdisf` | `__ll_to_f` | llcvt.c |
| 0x80009B94 | `__floatundidf` | `__ull_to_d` | llcvt.c |
| 0x80009BC8 | `__floatundisf` | `__ull_to_f` | llcvt.c |

## Further corrections

| vram | former label | ultralib name | file | confidence |
|---|---|---|---|---|
| 0x800086F0 | `osWritebackDCache_full` | `osSyncPrintf` | libc/syncprintf.c | medium (FINALROM empty varargs; segment position, as rmonPrintf) |
| 0x8000C050 | `dll_remove` | `__osDequeueThread` | os/thread.c | high |
| 0x8000C308 | `dll_insert` | `__osInsertTimer` | os/timerintr.c | high |
| 0x8000CFC4 | `__osCleanupThread` | `__osEnqueueAndYield` | os/exceptasm.s | high |
| 0x8000DBC0 | `osPiInit` | `__osPiCreateAccessQueue` | io/piacs.c | high |
| 0x8000E390 | `__osInsertTimer` | `osPiGetCmdQueue` | io/pigetcmdq.c | high |

| vram | former label | ultralib name | file |
|---|---|---|---|
| 0x80009E18 | `__osTimerInterrupt` | `__osPfsCheckRamArea` (static) | io/pfsinitpak.c |
| 0x8000C090 | `dll_init` | `__osTimerServicesInit` | os/timerintr.c |
| 0x8000C11C | `dll_update` | `__osTimerInterrupt` | os/timerintr.c |
| 0x8000C294 | `dll_reschedule` | `__osSetTimerIntr` | os/timerintr.c |

| vram | former label | ultralib name | file |
|---|---|---|---|
| 0x8000EC70 | `__osRepairId` | `__osRepairPackId` | io/contpfs.c |
| 0x8000EFC0 | `__osCheckId` | `__osCheckPackId` | io/contpfs.c |
| 0x8000F2D0 | `osPfsReadWriteFile_pages` | `__osCheckId` | io/contpfs.c |
| 0x80037C50 | `gViTimeAccumHi` | `__osCurrentTime` (OSTime, 8 B) | os/timerintr.c |
| 0x80037C58 | `gViLastCount` | `__osBaseCounter` | os/timerintr.c |
| 0x80037C5C | `gViRetraceCount` | `__osViIntrCount` | os/timerintr.c |

| vram | former label | ultralib name | file |
|---|---|---|---|
| 0x80007840 | `osViModeTableGet` | `_VirtualToPhysicalTask` (static) | io/sptask.c |
| 0x8000D680 | `__osSpDma` | `__osSpRawStartDma` | io/sprawdma.c |
| 0x800095AC | `__osContGetStatus` | `__osContGetInitData` | io/controller.c |
| 0x80009658 | `__osContRamReset` | `__osPackRequestData` | io/controller.c |
| 0x8000E560 | `osSiInit` | `__osSiCreateAccessQueue` | io/siacs.c |
| 0x8000E6F0 | `__osContBuildRequest` | `__osPfsRequestOneChannel` | io/pfsgetstatus.c |
| 0x8000E7B4 | `__osContParseResponse` | `__osPfsGetOneChannelData` | io/pfsgetstatus.c |

| vram | former label | ultralib name | file |
|---|---|---|---|
| 0x8000C980 | `__osException` | `__osExceptionPreamble` (the 16 B vector stub) | os/exceptasm.s |
| 0x8000C990 | `__osExceptionPreamble` | `__osException` | os/exceptasm.s |
| 0x8000CEDC | `__osSendInterrupt` | `send_mesg` (the slot also holds `handle_CpU`) | os/exceptasm.s |
| 0x8000D298 | `__osExceptionPanic` | `__osCleanupThread` | os/exceptasm.s |
| 0x8000D840 | `__osTlbFlush` | `osUnmapTLBAll` | os/unmaptlball.s |
| 0x8000D890 | `__osTlbInit` | `osMapTLBRdb` | os/maptlbrdb.s |
| 0x8000DBB0 | `osCauseGet` | `__osGetCause` | os/getcause.s |

Notes:

* `__osCurrentTime` (0x80037C50) is an `OSTime`; its low word at 0x80037C54 is addressed as
  `%hi/%lo(__osCurrentTime + 0x4)`, as ultralib's 64-bit accesses are.
* The runtime images' symbol tables (`asm/us/ovl_{a,b}/symbols.json`) are snapshots and still spell some former
  labels; nothing in the runtime images calls those functions.
* The game image's symbol table (`asm/us/blob/symbols.json`) still spells the seven exceptasm/TLB/cause
  accessors by their former labels; game C that calls them uses those spellings. The addresses are the same.
