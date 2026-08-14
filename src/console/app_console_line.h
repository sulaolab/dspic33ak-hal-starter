#ifndef APP_CONSOLE_LINE_H
#define APP_CONSOLE_LINE_H

#include <stdbool.h>

/*
 * Adapter from this starter's line console to sonora's module-message grammar.
 *
 * The touch bring-up console (module 'k') is ported from sonora unchanged, because
 * the tuning manual it belongs to is a procedure written in those exact command
 * strings: "*kz, tap ten times, ?ko" has to be true on both boards or the document
 * only applies to one of them. sonora dispatches
 * <kind><module><name><hex pairs> into an app_console_msg_t; this starter has a
 * strcmp chain over whole lines. Rather than translate every command name and
 * fork the document, this file reproduces the one piece of sonora's console that
 * the ported module needs: the parse.
 *
 * Nothing else in the starter uses it. Existing commands (*fua5, ?fp, *tq0001)
 * keep going through fw_command.c's own chain, which is checked first, so this
 * only ever sees lines that chain did not claim.
 */

/* Parse one already-lowercased, NUL-terminated command line and hand it to the
 * module that owns its module letter. Returns false if the line is not a module
 * message at all or names a module nobody implements, in which case the caller
 * still owns the "unknown command" reply. */
bool app_console_line_dispatch(const char *line);

#endif /* APP_CONSOLE_LINE_H */
