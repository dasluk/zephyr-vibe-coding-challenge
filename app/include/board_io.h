#pragma once

/*
 * Board I/O: buzzer + button primitives.
 *
 * buzzer_request() is declared in controller_ipc.h (the frozen shared
 * contract) since communication calls it directly; it is implemented in
 * board_io_buzzer.c. This header only carries board-I/O-internal helpers
 * that other components don't need.
 */

#include "controller_ipc.h"

/**
 * @brief Initialize the board I/O buzzer PWM output.
 *
 * Must be called once at startup (e.g. from main()) before the first
 * buzzer_request() call. Safe to call again; re-initialization is a no-op
 * if already done.
 *
 * @return 0 on success, negative errno on failure.
 */
int board_io_buzzer_init(void);

/**
 * @brief Initialize the board I/O button (gpio-keys sw0) input + debounce.
 *
 * Configures the interrupt and debounce work; confirmed press/release
 * transitions are pushed onto button_evt_q from controller_ipc.h.
 *
 * @return 0 on success, negative errno on failure.
 */
int board_io_button_init(void);
