#include "comms_codec.h"

#include <stdio.h>
#include <string.h>

int comms_encode_hello(char *buf, size_t buf_len, const char *controller_id,
			const char *fw_version)
{
	if (!buf || !controller_id || !fw_version) {
		return -1;
	}
	return snprintf(buf, buf_len, "HELLO %s %s", controller_id, fw_version);
}

int comms_encode_btn(char *buf, size_t buf_len, uint8_t id, bool pressed, int64_t t_ms)
{
	if (!buf) {
		return -1;
	}
	return snprintf(buf, buf_len, "BTN %u %s %lld", (unsigned int)id,
			 pressed ? "DOWN" : "UP", (long long)t_ms);
}

int comms_encode_gesture(char *buf, size_t buf_len, const char *name, float confidence,
			  int64_t t_ms)
{
	if (!buf || !name) {
		return -1;
	}
	return snprintf(buf, buf_len, "GESTURE %s %.3f %lld", name, (double)confidence,
			 (long long)t_ms);
}

int comms_encode_axis(char *buf, size_t buf_len, const char *name, float value, int64_t t_ms)
{
	if (!buf || !name) {
		return -1;
	}
	return snprintf(buf, buf_len, "AXIS %s %.3f %lld", name, (double)value,
			 (long long)t_ms);
}

int comms_encode_pong(char *buf, size_t buf_len, int64_t t_ms)
{
	if (!buf) {
		return -1;
	}
	return snprintf(buf, buf_len, "PONG %lld", (long long)t_ms);
}

int comms_decode_line(const char *line, struct comms_cmd *out)
{
	if (!line || !out) {
		return -1;
	}

	char work[COMMS_CODEC_MAX_LINE];
	size_t len = 0;

	while (line[len] != '\0' && len < sizeof(work) - 1) {
		work[len] = line[len];
		len++;
	}
	work[len] = '\0';

	/* Strip trailing CR/LF (frames may arrive as "...\r\n" or "...\n"). */
	while (len > 0 && (work[len - 1] == '\n' || work[len - 1] == '\r')) {
		work[--len] = '\0';
	}

	out->type = COMMS_CMD_UNKNOWN;

	char keyword[16];
	int consumed = 0;

	if (sscanf(work, "%15s%n", keyword, &consumed) != 1) {
		/* Empty/whitespace-only line: unknown, not an error. */
		return 0;
	}

	const char *rest = work + consumed;

	if (strcmp(keyword, "BUZZ") == 0) {
		unsigned long freq_hz = 0;
		unsigned long duration_ms = 0;

		if (sscanf(rest, "%lu %lu", &freq_hz, &duration_ms) == 2) {
			out->type = COMMS_CMD_BUZZ;
			out->buzz.freq_hz = (uint32_t)freq_hz;
			out->buzz.duration_ms = (uint32_t)duration_ms;
		}
	} else if (strcmp(keyword, "PING") == 0) {
		long long t_ms = 0;

		if (sscanf(rest, "%lld", &t_ms) == 1) {
			out->type = COMMS_CMD_PING;
			out->ping.t_ms = (int64_t)t_ms;
		}
	} else if (strcmp(keyword, "RESET") == 0) {
		out->type = COMMS_CMD_RESET;
	}
	/* Anything else (unrecognized keyword, or a recognized keyword with a
	 * malformed body) leaves out->type == COMMS_CMD_UNKNOWN. The caller
	 * logs and ignores it; this function never fails on bad input, only
	 * on NULL arguments.
	 */

	return 0;
}
