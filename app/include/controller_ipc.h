#pragma once
#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/* sensor driver -> fusion */
struct imu_sample {
    int64_t t_ms;
    float ax, ay, az;   /* g */
    float gx, gy, gz;   /* deg/s */
};

/* board I/O -> communication (buttons are already discrete/debounced) */
struct button_event {
    uint8_t id;
    bool pressed;
    int64_t t_ms;
};

/* fusion -> communication */
enum controller_evt_type { EVT_GESTURE, EVT_AXIS };
enum gesture_id {
    GESTURE_SWING_UP, GESTURE_SWING_DOWN,
    GESTURE_SWING_LEFT, GESTURE_SWING_RIGHT,
    GESTURE_SHAKE, GESTURE_PUNCH,
};

struct controller_event {
    enum controller_evt_type type;
    int64_t t_ms;
    union {
        struct { enum gesture_id id; float confidence; } gesture;
        struct { const char *name; float value; } axis; /* e.g. "tilt_x" */
    };
};

/* communication -> board I/O (host BUZZ command) */
void buzzer_request(uint32_t freq_hz, uint32_t duration_ms);

extern struct k_msgq imu_sample_q;      /* sensor driver produces, fusion consumes */
extern struct k_msgq button_evt_q;      /* board I/O produces, communication consumes */
extern struct k_msgq controller_evt_q;  /* fusion produces, communication consumes */
