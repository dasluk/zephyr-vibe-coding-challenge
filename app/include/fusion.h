#pragma once

/*
 * Fusion / gesture-recognition module.
 *
 * Consumes struct imu_sample from imu_sample_q and produces
 * struct controller_event onto controller_evt_q (both frozen in
 * controller_ipc.h):
 *
 *   - EVT_GESTURE: one of GESTURE_SWING_UP/DOWN/LEFT/RIGHT, GESTURE_SHAKE,
 *     GESTURE_PUNCH, one event per distinct motion (debounced/cooldown'd,
 *     not a flood of repeats).
 *   - EVT_AXIS "tilt_x": a low-pass-filtered roll value in [-1.0, 1.0],
 *     published at a throttled rate (see FUSION_TILT_PUBLISH_MS).
 *
 * The processing thread is started automatically at boot via
 * K_THREAD_DEFINE in fusion.c; there is no explicit init/start API for
 * main.c to call. This header is a placeholder for any future public
 * surface (e.g. if a supervisor ever needs to query fusion state), and
 * documents the module's inputs/outputs.
 *
 * Tunable thresholds/timings live in fusion_tuning.h, not here.
 */
