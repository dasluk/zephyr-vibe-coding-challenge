#ifndef COMMS_CODEC_H_
#define COMMS_CODEC_H_

/*
 * Pure line-formatting/parsing for the board<->host ASCII serial protocol
 * described in intent.md / specs/communication.spec.md.
 *
 * Deliberately hardware- and Zephyr-independent (only <stdint.h>/<stdbool.h>/
 * <stddef.h>) so it can be built and unit-tested with a plain host compiler,
 * with no UART/kernel dependency. The actual I/O ("transport") lives in
 * comms_tx.c / comms_rx.c.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Big enough for any line this protocol emits/accepts, plus '\n'+'\0'. */
#define COMMS_CODEC_MAX_LINE 96

/* ---------------- Encode: board -> host ---------------- */

/*
 * Each comms_encode_* function renders one line WITHOUT a trailing newline
 * into buf (NUL-terminated). Returns the number of characters that would
 * have been written (as snprintf does), or a negative value on a bad
 * argument (e.g. NULL buf/name). Callers should treat a return >= buf_len
 * as truncation.
 */
int comms_encode_hello(char *buf, size_t buf_len, const char *controller_id,
			const char *fw_version);
int comms_encode_btn(char *buf, size_t buf_len, uint8_t id, bool pressed, int64_t t_ms);
int comms_encode_gesture(char *buf, size_t buf_len, const char *name, float confidence,
			  int64_t t_ms);
int comms_encode_axis(char *buf, size_t buf_len, const char *name, float value, int64_t t_ms);
int comms_encode_pong(char *buf, size_t buf_len, int64_t t_ms);

/* ---------------- Decode: host -> board ---------------- */

enum comms_cmd_type {
	COMMS_CMD_BUZZ,
	COMMS_CMD_PING,
	COMMS_CMD_RESET,
	COMMS_CMD_UNKNOWN, /* malformed or unrecognized: log + ignore, never fatal */
};

struct comms_cmd {
	enum comms_cmd_type type;
	union {
		struct {
			uint32_t freq_hz;
			uint32_t duration_ms;
		} buzz;
		struct {
			int64_t t_ms;
		} ping;
	};
};

/*
 * Parses one line (a NUL-terminated string; a trailing '\n' and/or '\r' is
 * tolerated and stripped, not required). Always fills *out and returns 0,
 * EXCEPT when line or out is NULL, in which case it returns -1 and leaves
 * *out untouched. A recognized keyword with a malformed body (wrong
 * argument count/type) or an unrecognized keyword both yield
 * out->type == COMMS_CMD_UNKNOWN (return value is still 0) -- callers
 * decide how to log that, but must never treat it as fatal.
 */
int comms_decode_line(const char *line, struct comms_cmd *out);

#ifdef __cplusplus
}
#endif

#endif /* COMMS_CODEC_H_ */
