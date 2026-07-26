#ifndef FW_COMMAND_H
#define FW_COMMAND_H

#include <stdbool.h>

/* Minimal UART1 command processor for the beginner-facing Dual Bank workflow.
 *
 * Public commands (case-insensitive):
 *   *fua5  receive and validate an image into the inactive partition
 *   *fca5  commit the most recently validated image and reset to swap
 */
void fw_command_init(void);
void fw_command_poll(void);

/* True after *fua5 (and while accidental raw-file data is being discarded).
 * The application uses this to suppress every periodic console/demo message so
 * XMODEM control bytes remain the only traffic on UART1. */
bool fw_command_quiet(void);

#endif /* FW_COMMAND_H */
