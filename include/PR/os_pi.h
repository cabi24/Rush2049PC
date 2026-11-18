/* include/PR/os_pi.h: the directions of a transfer (libultra's os_pi.h), which the SI's raw DMA takes
 * (__osSiRawStartDma: the PIF's RAM written, or read back). The PC has no PI: the game reads the cartridge through
 * port/ultra (pc_cart_read, docs/port/ULTRA.md 11). */

#ifndef _OS_PI_H_
#define _OS_PI_H_

#include "types.h"
#include "PR/os_message.h"

#define OS_READ         0       /* the device's bytes into memory */
#define OS_WRITE        1       /* memory's bytes to the device */

#endif /* _OS_PI_H_ */
