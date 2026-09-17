/*
 * Ztest suite for the fusion/gesture module (app/src/fusion.c), run on
 * native_sim. Feeds synthetic imu_sample sequences through the real
 * imu_sample_q -> fusion thread -> controller_evt_q pipeline (the fusion
 * thread is auto-started via K_THREAD_DEFINE, exactly as it would be on
 * target) and asserts on the controller_events that come out.
 *
 * All phases run back-to-back on one continuous logical timeline (the
 * fusion thread's internal state machine persists across phases, just as
 * it would in normal continuous operation), separated by a generous idle
 * "settle" gap so each motion episode fully closes out (including its
 * post-gesture cooldown) before the next one begins.
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <string.h>

#include "controller_ipc.h"

#define SAMPLE_DT_MS 20

static int64_t t_cursor;

static void feed(float ax, float ay, float az)
{
    struct imu_sample s = {
        .t_ms = t_cursor,
        .ax = ax,
        .ay = ay,
        .az = az,
        .gx = 0.f,
        .gy = 0.f,
        .gz = 0.f,
    };
    int ret = k_msgq_put(&imu_sample_q, &s, K_MSEC(500));

    zassert_equal(ret, 0, "failed to enqueue imu_sample @ t=%lld", s.t_ms);
    t_cursor += SAMPLE_DT_MS;
}

static void feed_n(float ax, float ay, float az, int n)
{
    for (int i = 0; i < n; i++) {
        feed(ax, ay, az);
    }
}

/* >= 300ms (FUSION_GESTURE_COOLDOWN_MS) worth of at-rest samples: enough to
 * both close out a motion episode (FUSION_REST_CONFIRM_SAMPLES) and let any
 * resulting cooldown fully elapse before the next phase's motion begins. */
static void settle(void)
{
    feed_n(0.0f, 0.0f, 1.0f, 25); /* 25 * 20ms = 500ms */
}

#define MAX_EVENTS 64

/* Drain every controller_event produced so far. Waits generously for the
 * first one to arrive (the fusion thread runs as a separate, scheduled
 * thread) then drains the rest without waiting, since by then it has
 * already caught up (samples are consumed near-instantly in wall-clock
 * time; only the logical t_ms values are large). */
static int drain_events(struct controller_event *out, int max)
{
    int n = 0;

    while (n < max) {
        k_timeout_t wait = (n == 0) ? K_MSEC(500) : K_MSEC(100);

        if (k_msgq_get(&controller_evt_q, &out[n], wait) != 0) {
            break;
        }
        n++;
    }
    return n;
}

static int count_gestures(struct controller_event *evts, int n, enum gesture_id *first_id)
{
    int count = 0;

    for (int i = 0; i < n; i++) {
        if (evts[i].type == EVT_GESTURE) {
            if (count == 0 && first_id != NULL) {
                *first_id = evts[i].gesture.id;
            }
            count++;
        }
    }
    return count;
}

ZTEST_SUITE(fusion, NULL, NULL, NULL, NULL, NULL);

ZTEST(fusion, test_gesture_recognition_sequence)
{
    struct controller_event evts[MAX_EVENTS];
    int n, gestures;
    enum gesture_id first;

    /* --- Phase 1: resting -- must NOT produce any gesture event. --- */
    feed_n(0.0f, 0.0f, 1.0f, 40); /* 800ms of the board lying still */
    n = drain_events(evts, MAX_EVENTS);
    gestures = count_gestures(evts, n, &first);
    zassert_equal(gestures, 0, "resting trace produced %d spurious gesture event(s)",
                  gestures);

    /* --- Phase 2: swing right -- single ramp-up/ramp-down on +ay, no
     * direction reversal. Expect exactly one SWING_RIGHT, not a flood. */
    feed(0.10f, 0.10f, 1.0f);
    feed(0.10f, 0.25f, 1.0f);
    feed(0.10f, 0.45f, 1.0f);
    feed(0.10f, 0.65f, 1.0f);
    feed(0.10f, 0.90f, 1.0f);
    feed(0.10f, 0.90f, 1.0f);
    feed(0.10f, 0.60f, 1.0f);
    feed(0.10f, 0.30f, 1.0f);
    feed(0.10f, 0.10f, 1.0f);
    settle();
    n = drain_events(evts, MAX_EVENTS);
    gestures = count_gestures(evts, n, &first);
    zassert_equal(gestures, 1, "swing-right trace produced %d gesture event(s), expected 1",
                  gestures);
    zassert_equal(first, GESTURE_SWING_RIGHT, "swing-right trace classified as %d", first);

    /* --- Phase 3: shake -- rapid sign reversal on the same axis (+-0.6
     * on ax). Expect exactly one SHAKE, not one event per reversal. --- */
    feed(0.6f, 0.0f, 1.0f);
    feed(-0.6f, 0.0f, 1.0f);
    feed(0.6f, 0.0f, 1.0f);
    feed(-0.6f, 0.0f, 1.0f);
    feed(0.6f, 0.0f, 1.0f);
    feed(-0.6f, 0.0f, 1.0f);
    settle();
    n = drain_events(evts, MAX_EVENTS);
    gestures = count_gestures(evts, n, &first);
    zassert_equal(gestures, 1, "shake trace produced %d gesture event(s), expected 1",
                  gestures);
    zassert_equal(first, GESTURE_SHAKE, "shake trace classified as %d", first);

    /* --- Phase 4 (bonus): punch -- same ramp shape as phase 2 but on the
     * forward axis (ax). Expect exactly one PUNCH. --- */
    feed(0.10f, 0.0f, 1.0f);
    feed(0.25f, 0.0f, 1.0f);
    feed(0.45f, 0.0f, 1.0f);
    feed(0.65f, 0.0f, 1.0f);
    feed(0.90f, 0.0f, 1.0f);
    feed(0.90f, 0.0f, 1.0f);
    feed(0.60f, 0.0f, 1.0f);
    feed(0.30f, 0.0f, 1.0f);
    feed(0.10f, 0.0f, 1.0f);
    settle();
    n = drain_events(evts, MAX_EVENTS);
    gestures = count_gestures(evts, n, &first);
    zassert_equal(gestures, 1, "punch trace produced %d gesture event(s), expected 1", gestures);
    zassert_equal(first, GESTURE_PUNCH, "punch trace classified as %d", first);

    /* --- Phase 5 (bonus): swing up -- same ramp shape again but on the
     * vertical axis (az, deviation from the 1g baseline). Peak deviation
     * is negative (az dips below the 1g baseline) since real-hardware
     * testing confirmed a physical swing up reads that way, not positive
     * -- see fusion_tuning.h's axis convention note. Expect exactly one
     * SWING_UP. --- */
    feed(0.0f, 0.0f, 0.90f);
    feed(0.0f, 0.0f, 0.75f);
    feed(0.0f, 0.0f, 0.55f);
    feed(0.0f, 0.0f, 0.35f);
    feed(0.0f, 0.0f, 0.10f);
    feed(0.0f, 0.0f, 0.10f);
    feed(0.0f, 0.0f, 0.40f);
    feed(0.0f, 0.0f, 0.70f);
    feed(0.0f, 0.0f, 0.90f);
    settle();
    n = drain_events(evts, MAX_EVENTS);
    gestures = count_gestures(evts, n, &first);
    zassert_equal(gestures, 1, "swing-up trace produced %d gesture event(s), expected 1",
                  gestures);
    zassert_equal(first, GESTURE_SWING_UP, "swing-up trace classified as %d", first);
}

ZTEST(fusion, test_tilt_axis_is_published_and_throttled)
{
    struct controller_event evts[MAX_EVENTS];
    int n, axis_events = 0;
    bool saw_named_tilt_x = false;

    /* A gentle, sustained tilt (well under the gesture motion threshold)
     * should show up as periodic EVT_AXIS "tilt_x" events, throttled to
     * roughly FUSION_TILT_PUBLISH_MS, not one event per sample. */
    feed_n(0.0f, 0.30f, 0.95f, 30); /* 600ms of a steady partial tilt */
    settle();

    n = drain_events(evts, MAX_EVENTS);
    for (int i = 0; i < n; i++) {
        if (evts[i].type == EVT_AXIS) {
            axis_events++;
            if (evts[i].axis.name != NULL &&
                strcmp(evts[i].axis.name, "tilt_x") == 0) {
                saw_named_tilt_x = true;
                zassert_true(evts[i].axis.value >= -1.0f && evts[i].axis.value <= 1.0f,
                             "tilt_x value %f out of normalized range",
                             (double)evts[i].axis.value);
            }
        }
    }

    zassert_true(saw_named_tilt_x, "expected at least one tilt_x axis event");
    /* ~1100ms of samples at a ~50ms publish period should yield well under
     * one event per sample (40 samples fed) -- throttling is in effect. */
    zassert_true(axis_events < 40, "tilt_x axis events (%d) not throttled below sample count",
                axis_events);
}
