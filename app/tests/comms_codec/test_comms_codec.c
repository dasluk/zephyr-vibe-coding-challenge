/*
 * Plain host-side unit test for the comms codec (app/src/comms_codec.c).
 *
 * Deliberately NOT a Zephyr/ztest binary: the codec is pure C with no
 * Zephyr/hardware dependency, so it's built and run directly with the host
 * compiler, independent of any UART transport or native_sim. See
 * specs/communication.spec.md "Testing".
 *
 * Build + run:
 *   gcc -std=c11 -Wall -Wextra -I app/include \
 *       app/src/comms_codec.c app/tests/comms_codec/test_comms_codec.c \
 *       -o /tmp/test_comms_codec && /tmp/test_comms_codec
 * (or: app/tests/comms_codec/run.sh)
 */

#include "comms_codec.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                                    \
	do {                                                                             \
		g_checks++;                                                             \
		if (!(cond)) {                                                          \
			g_failures++;                                                   \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
		}                                                                       \
	} while (0)

#define CHECK_STREQ(a, b)                                                               \
	do {                                                                             \
		g_checks++;                                                             \
		if (strcmp((a), (b)) != 0) {                                           \
			g_failures++;                                                   \
			printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,   \
			       (a), (b));                                               \
		}                                                                       \
	} while (0)

static void test_encode_hello(void)
{
	char buf[COMMS_CODEC_MAX_LINE];
	int n = comms_encode_hello(buf, sizeof(buf), "ctrl-A", "0.1.0");

	CHECK(n > 0);
	CHECK_STREQ(buf, "HELLO ctrl-A 0.1.0");
}

static void test_encode_btn(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_btn(buf, sizeof(buf), 1, true, 1241);
	CHECK_STREQ(buf, "BTN 1 DOWN 1241");

	comms_encode_btn(buf, sizeof(buf), 1, false, 1305);
	CHECK_STREQ(buf, "BTN 1 UP 1305");
}

static void test_encode_gesture(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_gesture(buf, sizeof(buf), "SWING_RIGHT", 0.87f, 1240);
	CHECK_STREQ(buf, "GESTURE SWING_RIGHT 0.870 1240");
}

static void test_encode_axis(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_axis(buf, sizeof(buf), "tilt_x", 0.42f, 1234);
	CHECK_STREQ(buf, "AXIS tilt_x 0.420 1234");
}

static void test_encode_pong(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	comms_encode_pong(buf, sizeof(buf), 5000);
	CHECK_STREQ(buf, "PONG 5000");
}

static void test_encode_null_args(void)
{
	char buf[COMMS_CODEC_MAX_LINE];

	CHECK(comms_encode_hello(NULL, sizeof(buf), "a", "b") < 0);
	CHECK(comms_encode_hello(buf, sizeof(buf), NULL, "b") < 0);
	CHECK(comms_encode_btn(NULL, sizeof(buf), 1, true, 0) < 0);
	CHECK(comms_encode_gesture(buf, sizeof(buf), NULL, 0.f, 0) < 0);
	CHECK(comms_encode_axis(buf, sizeof(buf), NULL, 0.f, 0) < 0);
	CHECK(comms_encode_pong(NULL, sizeof(buf), 0) < 0);
}

static void test_encode_truncation_reported(void)
{
	char tiny[4];
	int n = comms_encode_hello(tiny, sizeof(tiny), "ctrl-A", "0.1.0");

	/* snprintf-style: return value is the length that *would* have been
	 * written, so callers can detect truncation even though tiny[] only
	 * holds a few bytes.
	 */
	CHECK(n >= (int)sizeof(tiny));
}

static void test_decode_buzz(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("BUZZ 440 200", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_BUZZ);
	CHECK(cmd.buzz.freq_hz == 440);
	CHECK(cmd.buzz.duration_ms == 200);
}

static void test_decode_buzz_with_crlf(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("BUZZ 880 50\r\n", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_BUZZ);
	CHECK(cmd.buzz.freq_hz == 880);
	CHECK(cmd.buzz.duration_ms == 50);
}

static void test_decode_ping(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("PING 12345", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_PING);
	CHECK(cmd.ping.t_ms == 12345);
}

static void test_decode_reset(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("RESET", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_RESET);
}

static void test_decode_malformed_is_unknown_not_error(void)
{
	struct comms_cmd cmd;

	/* Recognized keyword, wrong arity -> UNKNOWN, but decode itself still
	 * "succeeds" (returns 0): malformed input must never be treated as a
	 * crash/link-wedging condition.
	 */
	CHECK(comms_decode_line("BUZZ 440", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);

	CHECK(comms_decode_line("BUZZ not_a_number 200", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);

	CHECK(comms_decode_line("PING", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);
}

static void test_decode_unrecognized_keyword(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("FROBNICATE 1 2 3", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);
}

static void test_decode_empty_line(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line("", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);

	CHECK(comms_decode_line("\r\n", &cmd) == 0);
	CHECK(cmd.type == COMMS_CMD_UNKNOWN);
}

static void test_decode_null_args(void)
{
	struct comms_cmd cmd;

	CHECK(comms_decode_line(NULL, &cmd) < 0);
	CHECK(comms_decode_line("PING 1", NULL) < 0);
}

int main(void)
{
	test_encode_hello();
	test_encode_btn();
	test_encode_gesture();
	test_encode_axis();
	test_encode_pong();
	test_encode_null_args();
	test_encode_truncation_reported();
	test_decode_buzz();
	test_decode_buzz_with_crlf();
	test_decode_ping();
	test_decode_reset();
	test_decode_malformed_is_unknown_not_error();
	test_decode_unrecognized_keyword();
	test_decode_empty_line();
	test_decode_null_args();

	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
