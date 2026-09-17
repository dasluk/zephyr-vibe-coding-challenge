#include "comms_transport.h"
#include "comms_codec.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/logging/log.h>

#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(comms_transport, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * The protocol link is the board's existing console UART (USART1, wired up
 * in boards/demo/demo.dts as zephyr,console). We deliberately do NOT go
 * through the console/logging subsystem for it -- see
 * specs/communication.spec.md and docs/task-distribution.md's "serial
 * protocol vs logging" decision. Logging is on RTT instead (prj.conf).
 */
#define COMMS_UART_NODE DT_CHOSEN(zephyr_console)

static const struct device *comms_uart_dev;

/* One-shot readiness latch: gives once comms_transport_init() has run, so
 * comms_rx's thread can wait on it regardless of whether comms_tx's or
 * comms_rx's auto-started thread happens to run first.
 */
K_SEM_DEFINE(comms_transport_ready_sem, 0, 1);

RING_BUF_DECLARE(comms_tx_ring, 256);

struct comms_rx_line {
	char line[COMMS_CODEC_MAX_LINE];
};

K_MSGQ_DEFINE(comms_rx_line_q, sizeof(struct comms_rx_line), 8, 4);

/* Only ever touched from the UART ISR (single producer), so no locking
 * needed for the assembly buffer itself.
 */
static char comms_rx_assembly[COMMS_CODEC_MAX_LINE];
static size_t comms_rx_assembly_len;

static void comms_rx_assembly_push_line(void)
{
	struct comms_rx_line msg;

	comms_rx_assembly[comms_rx_assembly_len] = '\0';
	memcpy(msg.line, comms_rx_assembly, comms_rx_assembly_len + 1);
	comms_rx_assembly_len = 0;

	if (k_msgq_put(&comms_rx_line_q, &msg, K_NO_WAIT) != 0) {
		/* RX thread fell behind: drop the oldest queued line rather
		 * than blocking the ISR or losing the newest one silently.
		 */
		struct comms_rx_line discard;

		(void)k_msgq_get(&comms_rx_line_q, &discard, K_NO_WAIT);
		(void)k_msgq_put(&comms_rx_line_q, &msg, K_NO_WAIT);
	}
}

static void comms_uart_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
		if (uart_irq_tx_ready(dev)) {
			uint8_t byte;

			if (ring_buf_get(&comms_tx_ring, &byte, 1) == 1) {
				uart_fifo_fill(dev, &byte, 1);
			} else {
				uart_irq_tx_disable(dev);
			}
		}

		if (uart_irq_rx_ready(dev)) {
			uint8_t byte;

			while (uart_fifo_read(dev, &byte, 1) == 1) {
				if (byte == '\n') {
					comms_rx_assembly_push_line();
				} else if (byte != '\r') {
					if (comms_rx_assembly_len <
					    sizeof(comms_rx_assembly) - 1) {
						comms_rx_assembly[comms_rx_assembly_len++] =
							(char)byte;
					} else {
						/* Line too long: resync on the next
						 * '\n' instead of overflowing or
						 * ever crashing.
						 */
						comms_rx_assembly_len = 0;
					}
				}
			}
		}
	}
}

int comms_transport_init(void)
{
	comms_uart_dev = DEVICE_DT_GET(COMMS_UART_NODE);

	if (!device_is_ready(comms_uart_dev)) {
		LOG_ERR("comms UART device not ready");
		/* Still latch "ready" so comms_rx doesn't block forever on a
		 * dead link -- its calls into a not-ready device will just be
		 * no-ops/errors from here on, logged via RTT, never a hang.
		 */
		k_sem_give(&comms_transport_ready_sem);
		return -ENODEV;
	}

	uart_irq_callback_user_data_set(comms_uart_dev, comms_uart_isr, NULL);
	uart_irq_rx_enable(comms_uart_dev);

	k_sem_give(&comms_transport_ready_sem);
	return 0;
}

void comms_transport_wait_ready(void)
{
	/* Take-then-give: this is a one-shot "has init happened yet" latch,
	 * not a mutex, so every waiter (there's only ever one -- comms_rx)
	 * must see it stay signaled forever after the first give.
	 */
	k_sem_take(&comms_transport_ready_sem, K_FOREVER);
	k_sem_give(&comms_transport_ready_sem);
}

void comms_transport_send_line(const char *line)
{
	size_t len = strlen(line);
	uint32_t put = ring_buf_put(&comms_tx_ring, (const uint8_t *)line, len);
	const uint8_t nl = '\n';

	if (put == len) {
		put += ring_buf_put(&comms_tx_ring, &nl, 1);
	}

	if (put != len + 1) {
		LOG_WRN("TX ring full, dropped line (wanted %u bytes, wrote %u)",
			(unsigned int)(len + 1), (unsigned int)put);
	}

	uart_irq_tx_enable(comms_uart_dev);
}

void comms_transport_recv_line(char *buf, size_t buf_len)
{
	struct comms_rx_line msg;

	k_msgq_get(&comms_rx_line_q, &msg, K_FOREVER);

	size_t n = strlen(msg.line);

	if (n >= buf_len) {
		n = buf_len - 1;
	}
	memcpy(buf, msg.line, n);
	buf[n] = '\0';
}
