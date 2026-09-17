#ifndef COMMS_TRANSPORT_H_
#define COMMS_TRANSPORT_H_

/*
 * The UART I/O ("transport") half of the board<->host protocol link.
 * Talks to the protocol UART (chosen zephyr,console -- USART1 on the demo
 * board) directly via the interrupt-driven UART driver API, bypassing the
 * console/logging subsystem entirely, so nothing but protocol lines ever
 * hits the wire. See specs/communication.spec.md.
 *
 * Line framing/parsing itself lives in comms_codec.h/.c, which knows
 * nothing about the UART; this module knows nothing about the protocol
 * grammar.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Binds the protocol UART device and starts interrupt-driven RX. Must be
 * called once (from comms_tx or wherever the app wires threads up) before
 * comms_transport_send_line()/recv_line(). Returns 0 on success, a negative
 * errno if the device isn't ready.
 */
int comms_transport_init(void);

/*
 * Blocks the calling thread until comms_transport_init() has completed
 * (successfully or not). Lets comms_rx start its RX loop only once the
 * device is bound, without caring which of comms_tx/comms_rx's
 * auto-started threads happens to run first.
 */
void comms_transport_wait_ready(void);

/*
 * Queues one already-formatted protocol line (no trailing '\n' -- this
 * function appends it) for interrupt-driven transmission. Only meant to be
 * called from a single producer thread (comms_tx); safe to call
 * concurrently with comms_transport_recv_line() from another thread.
 */
void comms_transport_send_line(const char *line);

/*
 * Blocks the calling thread until one complete line has arrived from the
 * UART, and copies it (NUL-terminated, CR/LF already stripped) into buf
 * (truncated to buf_len - 1 bytes if necessary). Only meant to be called
 * from a single consumer thread (comms_rx).
 */
void comms_transport_recv_line(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* COMMS_TRANSPORT_H_ */
