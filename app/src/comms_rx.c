/*
 * RX side of the board<->host protocol link: parses incoming lines and
 * dispatches BUZZ/PING/RESET. Malformed or unrecognized lines are logged
 * (via RTT) and ignored -- must never crash or wedge the link. See
 * specs/communication.spec.md.
 */

#include "comms_transport.h"
#include "comms_codec.h"
#include "controller_ipc.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(comms_rx, CONFIG_LOG_DEFAULT_LEVEL);

static void handle_line(const char *line)
{
	struct comms_cmd cmd;

	if (comms_decode_line(line, &cmd) != 0) {
		/* Only NULL args cause this, which can't happen with a valid
		 * local buffer, but stay defensive rather than assume.
		 */
		LOG_WRN("comms_decode_line rejected input");
		return;
	}

	switch (cmd.type) {
	case COMMS_CMD_BUZZ:
		/* board-io's implementation (feature/board-io branch); only
		 * the declaration in controller_ipc.h is available here.
		 */
		buzzer_request(cmd.buzz.freq_hz, cmd.buzz.duration_ms);
		break;

	case COMMS_CMD_PING: {
		char buf[COMMS_CODEC_MAX_LINE];

		comms_encode_pong(buf, sizeof(buf), cmd.ping.t_ms);
		comms_transport_send_line(buf);
		break;
	}

	case COMMS_CMD_RESET:
		LOG_WRN("RESET command received; rebooting");
		sys_reboot(SYS_REBOOT_WARM);
		break; /* unreachable */

	case COMMS_CMD_UNKNOWN:
	default:
		LOG_WRN("malformed/unrecognized line ignored: \"%s\"", line);
		break;
	}
}

static void comms_rx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Wait for comms_tx's thread to have bound the shared UART device,
	 * regardless of which of the two auto-started threads runs first.
	 */
	comms_transport_wait_ready();

	while (1) {
		char line[COMMS_CODEC_MAX_LINE];

		comms_transport_recv_line(line, sizeof(line));
		handle_line(line);
	}
}

K_THREAD_DEFINE(comms_rx_tid, 1280, comms_rx_thread, NULL, NULL, NULL, 7, 0, 0);
