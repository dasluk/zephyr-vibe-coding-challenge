/*
 * TX side of the board<->host protocol link: drains button_evt_q and
 * controller_evt_q (frozen contract in controller_ipc.h) and writes
 * HELLO/BTN/GESTURE/AXIS lines out over the UART. See
 * specs/communication.spec.md.
 */

#include "comms_transport.h"
#include "comms_codec.h"
#include "controller_ipc.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <string.h>

LOG_MODULE_REGISTER(comms_tx, CONFIG_LOG_DEFAULT_LEVEL);

#ifndef APP_FW_VERSION
#define APP_FW_VERSION "0.0.0-unknown"
#endif

/*
 * Defensive AXIS throttle: fusion already throttles its own publish rate
 * (fusion-gesture.spec.md), but communication defends the link
 * independently in case that ever regresses. Tracked per axis name (a
 * handful of small fixed slots is plenty -- currently only "tilt_x" is
 * defined) rather than globally, so a burst on one axis can't starve
 * another.
 */
#define COMMS_AXIS_MIN_PERIOD_MS 15
#define COMMS_AXIS_MAX_TRACKED 4

struct axis_throttle_slot {
	char name[16];
	int64_t last_sent_ms;
};

static struct axis_throttle_slot axis_slots[COMMS_AXIS_MAX_TRACKED];

static bool axis_throttle_allow(const char *name, int64_t t_ms)
{
	int free_slot = -1;

	for (int i = 0; i < COMMS_AXIS_MAX_TRACKED; i++) {
		if (axis_slots[i].name[0] == '\0') {
			if (free_slot < 0) {
				free_slot = i;
			}
			continue;
		}
		if (strncmp(axis_slots[i].name, name, sizeof(axis_slots[i].name)) == 0) {
			if ((t_ms - axis_slots[i].last_sent_ms) < COMMS_AXIS_MIN_PERIOD_MS) {
				return false;
			}
			axis_slots[i].last_sent_ms = t_ms;
			return true;
		}
	}

	/* New axis name. */
	if (free_slot >= 0) {
		strncpy(axis_slots[free_slot].name, name, sizeof(axis_slots[free_slot].name) - 1);
		axis_slots[free_slot].last_sent_ms = t_ms;
	}
	/* Table full: don't throttle an axis name we can't track rather than
	 * silently dropping it.
	 */
	return true;
}

static const char *gesture_name(enum gesture_id id)
{
	switch (id) {
	case GESTURE_SWING_UP:
		return "SWING_UP";
	case GESTURE_SWING_DOWN:
		return "SWING_DOWN";
	case GESTURE_SWING_LEFT:
		return "SWING_LEFT";
	case GESTURE_SWING_RIGHT:
		return "SWING_RIGHT";
	case GESTURE_SHAKE:
		return "SHAKE";
	case GESTURE_PUNCH:
		return "PUNCH";
	default:
		return "UNKNOWN";
	}
}

static void send_hello(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_hello(buf, sizeof(buf), CONFIG_CONTROLLER_ID, APP_FW_VERSION);
	comms_transport_send_line(buf);
}

static void handle_button_event(const struct button_event *evt)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_btn(buf, sizeof(buf), evt->id, evt->pressed, evt->t_ms);
	comms_transport_send_line(buf);
}

static void handle_controller_event(const struct controller_event *evt)
{
	char buf[COMMS_CODEC_MAX_LINE];

	switch (evt->type) {
	case EVT_GESTURE:
		comms_encode_gesture(buf, sizeof(buf), gesture_name(evt->gesture.id),
				     evt->gesture.confidence, evt->t_ms);
		comms_transport_send_line(buf);
		break;
	case EVT_AXIS:
		if (!axis_throttle_allow(evt->axis.name, evt->t_ms)) {
			break;
		}
		comms_encode_axis(buf, sizeof(buf), evt->axis.name, evt->axis.value, evt->t_ms);
		comms_transport_send_line(buf);
		break;
	default:
		LOG_WRN("unknown controller_event.type %d", (int)evt->type);
		break;
	}
}

static void comms_tx_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret = comms_transport_init();

	if (ret != 0) {
		LOG_ERR("comms transport init failed (%d); TX will be a no-op", ret);
	}

	send_hello();

	struct k_poll_event events[] = {
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						 K_POLL_MODE_NOTIFY_ONLY, &button_evt_q, 0),
		K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE,
						 K_POLL_MODE_NOTIFY_ONLY, &controller_evt_q, 0),
	};

	while (1) {
		int rc = k_poll(events, ARRAY_SIZE(events), K_MSEC(100));

		if (rc != 0 && rc != -EAGAIN) {
			LOG_WRN("k_poll on TX queues returned %d", rc);
		}

		struct button_event btn;

		while (k_msgq_get(&button_evt_q, &btn, K_NO_WAIT) == 0) {
			handle_button_event(&btn);
		}

		struct controller_event cev;

		while (k_msgq_get(&controller_evt_q, &cev, K_NO_WAIT) == 0) {
			handle_controller_event(&cev);
		}

		events[0].state = K_POLL_STATE_NOT_READY;
		events[1].state = K_POLL_STATE_NOT_READY;
	}
}

K_THREAD_DEFINE(comms_tx_tid, 1536, comms_tx_thread, NULL, NULL, NULL, 7, 0, 0);
