#ifndef FW_COMMAND_H
#define FW_COMMAND_H

#include <stdbool.h>

/* Minimal UART1 command processor for the beginner-facing dual-partition workflow.
 *
 * Public commands (case-insensitive):
 *   *fua5  receive and validate an image into the inactive partition
 *   *fca5  commit the most recently validated image and reset to swap
 */
void fw_command_init(void);
void fw_command_poll(void);

/* True during *fua5, after a successful receive while commit is pending, and
 * while accidental raw-file data is being discarded. A failed receive or
 * commit clears quiet after its diagnostics so normal starter output resumes. */
bool fw_command_quiet(void);

#endif /* FW_COMMAND_H */
