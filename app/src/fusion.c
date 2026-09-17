#include <zephyr/kernel.h>
#include <math.h>
#include <stdbool.h>

#include "controller_ipc.h"
#include "fusion.h"
#include "fusion_tuning.h"

#define FUSION_STACK_SIZE     1024
#define FUSION_THREAD_PRIORITY 5

#ifndef FUSION_PI
#define FUSION_PI 3.14159265358979323846f
#endif

enum fusion_state {
    FUSION_STATE_IDLE,
    FUSION_STATE_MOTION,
    FUSION_STATE_COOLDOWN,
};

static void fusion_thread_entry(void *p1, void *p2, void *p3);

K_THREAD_DEFINE(fusion_tid, FUSION_STACK_SIZE, fusion_thread_entry, NULL, NULL, NULL,
                 FUSION_THREAD_PRIORITY, 0, 0);

static inline float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static void publish_gesture(int64_t t_ms, enum gesture_id id, float confidence)
{
    struct controller_event evt = {
        .type = EVT_GESTURE,
        .t_ms = t_ms,
    };

    evt.gesture.id = id;
    evt.gesture.confidence = confidence;

    /* Best-effort: never let a backed-up link stall the fusion thread. A
     * dropped gesture is better than the whole pipeline falling behind. */
    (void)k_msgq_put(&controller_evt_q, &evt, K_NO_WAIT);
}

static void publish_axis(int64_t t_ms, const char *name, float value)
{
    struct controller_event evt = {
        .type = EVT_AXIS,
        .t_ms = t_ms,
    };

    evt.axis.name = name;
    evt.axis.value = value;

    (void)k_msgq_put(&controller_evt_q, &evt, K_NO_WAIT);
}

/*
 * Discrete gesture state machine.
 *
 * IDLE     -> waiting for accel deviation to exceed FUSION_MOTION_ENTER_G.
 * MOTION   -> tracking per-axis peaks and dominant-axis sign reversals
 *             until the signal settles back down (or times out), then
 *             classifies and emits exactly one controller_event.
 * COOLDOWN -> ignore further motion for FUSION_GESTURE_COOLDOWN_MS so one
 *             physical motion can't produce a flood of events.
 */
struct gesture_sm {
    enum fusion_state state;
    int64_t cooldown_until_ms;
    int64_t motion_start_ms;
    int rest_count;
    int reversal_count;
    float last_dom_sign;
    float peak_dx, peak_dy, peak_dz;
};

static void gesture_sm_enter_motion(struct gesture_sm *sm, int64_t t_ms, float dx, float dy,
                                     float dz)
{
    float adx = fabsf(dx), ady = fabsf(dy), adz = fabsf(dz);

    sm->state = FUSION_STATE_MOTION;
    sm->motion_start_ms = t_ms;
    sm->peak_dx = dx;
    sm->peak_dy = dy;
    sm->peak_dz = dz;
    sm->reversal_count = 0;
    sm->rest_count = 0;

    if (adx >= ady && adx >= adz) {
        sm->last_dom_sign = (dx >= 0.f) ? 1.f : -1.f;
    } else if (ady >= adx && ady >= adz) {
        sm->last_dom_sign = (dy >= 0.f) ? 1.f : -1.f;
    } else {
        sm->last_dom_sign = (dz >= 0.f) ? 1.f : -1.f;
    }
}

static void gesture_sm_classify_and_emit(struct gesture_sm *sm, int64_t t_ms)
{
    float apx = fabsf(sm->peak_dx), apy = fabsf(sm->peak_dy), apz = fabsf(sm->peak_dz);
    float peak_mag = sqrtf(sm->peak_dx * sm->peak_dx + sm->peak_dy * sm->peak_dy +
                            sm->peak_dz * sm->peak_dz);
    float confidence = clampf(peak_mag / FUSION_CONFIDENCE_SATURATE_G, 0.f, 1.f);
    enum gesture_id gid;

    if (sm->reversal_count >= FUSION_SHAKE_MIN_REVERSALS) {
        gid = GESTURE_SHAKE;
    } else if (apx >= apy && apx >= apz) {
        gid = GESTURE_PUNCH;
    } else if (apz >= apx && apz >= apy) {
        gid = (sm->peak_dz >= 0.f) ? GESTURE_SWING_UP : GESTURE_SWING_DOWN;
    } else {
        gid = (sm->peak_dy >= 0.f) ? GESTURE_SWING_RIGHT : GESTURE_SWING_LEFT;
    }

    publish_gesture(t_ms, gid, confidence);

    sm->state = FUSION_STATE_COOLDOWN;
    sm->cooldown_until_ms = t_ms + FUSION_GESTURE_COOLDOWN_MS;
}

static void gesture_sm_update(struct gesture_sm *sm, int64_t t_ms, float dx, float dy, float dz)
{
    float mag = sqrtf(dx * dx + dy * dy + dz * dz);

    switch (sm->state) {
    case FUSION_STATE_IDLE:
        if (mag >= FUSION_MOTION_ENTER_G) {
            gesture_sm_enter_motion(sm, t_ms, dx, dy, dz);
        }
        break;

    case FUSION_STATE_MOTION: {
        float adx = fabsf(dx), ady = fabsf(dy), adz = fabsf(dz);
        float dom_val;

        if (adx > fabsf(sm->peak_dx)) {
            sm->peak_dx = dx;
        }
        if (ady > fabsf(sm->peak_dy)) {
            sm->peak_dy = dy;
        }
        if (adz > fabsf(sm->peak_dz)) {
            sm->peak_dz = dz;
        }

        /* Track sign reversals on whichever axis is currently dominant;
         * a real shake flips sign repeatedly on the same axis. Simple
         * heuristic, not sophisticated by design (see spec). */
        if (adx >= ady && adx >= adz) {
            dom_val = dx;
        } else if (ady >= adx && ady >= adz) {
            dom_val = dy;
        } else {
            dom_val = dz;
        }
        if (fabsf(dom_val) >= FUSION_REVERSAL_SIGN_G) {
            float sign = (dom_val >= 0.f) ? 1.f : -1.f;

            if (sign != sm->last_dom_sign) {
                sm->reversal_count++;
                sm->last_dom_sign = sign;
            }
        }

        bool timed_out = (t_ms - sm->motion_start_ms) >= FUSION_MAX_MOTION_MS;

        if (mag < FUSION_MOTION_EXIT_G) {
            sm->rest_count++;
        } else {
            sm->rest_count = 0;
        }

        if (timed_out || sm->rest_count >= FUSION_REST_CONFIRM_SAMPLES) {
            gesture_sm_classify_and_emit(sm, t_ms);
        }
        break;
    }

    case FUSION_STATE_COOLDOWN:
        if (t_ms >= sm->cooldown_until_ms) {
            sm->state = FUSION_STATE_IDLE;
            sm->rest_count = 0;
        }
        break;
    }
}

/* Continuous tilt_x (roll) axis: low-pass filtered, throttled publish. */
struct tilt_filter {
    bool initialized;
    float filtered_deg;
    int64_t next_publish_ms;
};

static void tilt_update(struct tilt_filter *tf, int64_t t_ms, float ay, float az)
{
    float roll_deg = atan2f(ay, az) * (180.0f / FUSION_PI);

    if (!tf->initialized) {
        tf->filtered_deg = roll_deg;
        tf->initialized = true;
        tf->next_publish_ms = t_ms;
    } else {
        tf->filtered_deg += FUSION_TILT_LPF_ALPHA * (roll_deg - tf->filtered_deg);
    }

    if (t_ms >= tf->next_publish_ms) {
        float norm = clampf(tf->filtered_deg / FUSION_TILT_MAX_DEG, -1.0f, 1.0f);

        publish_axis(t_ms, "tilt_x", norm);
        tf->next_publish_ms = t_ms + FUSION_TILT_PUBLISH_MS;
    }
}

static void fusion_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    struct imu_sample s;
    struct gesture_sm sm = { .state = FUSION_STATE_IDLE };
    struct tilt_filter tf = { .initialized = false };

    while (1) {
        k_msgq_get(&imu_sample_q, &s, K_FOREVER);

        tilt_update(&tf, s.t_ms, s.ay, s.az);

        float dx = s.ax;
        float dy = s.ay;
        float dz = s.az - FUSION_GRAVITY_G;

        gesture_sm_update(&sm, s.t_ms, dx, dy, dz);
    }
}
