#pragma once

/*
 * Tunable constants for the fusion/gesture module (app/src/fusion.c).
 *
 * Centralized here so thresholds, filter coefficients, and timing windows
 * can be retuned against real motion data without touching the detection
 * logic itself. Expect these to need iteration once real hardware/mounting
 * is available (see fusion-gesture spec + final report open questions).
 *
 * All accel-deviation constants are in units of g, relative to the
 * assumed FUSION_GRAVITY_G resting baseline.
 *
 * --- Axis convention ---
 *   ax: forward(+) / back(-)          -> GESTURE_PUNCH   (still a heuristic
 *                                        guess, unconfirmed on hardware)
 *   ay: right(+)   / left(-)          -> GESTURE_SWING_RIGHT / _LEFT
 *                                        (still a heuristic guess, unconfirmed)
 *   az: DOWN(+)    / UP(-) (about the resting ~1g baseline)
 *                                     -> GESTURE_SWING_DOWN / _UP
 * The az mapping was confirmed INVERTED from the original guess by testing
 * on real hardware (a physical swing up peaks negative on dz, not
 * positive) and fixed in fusion.c's classifier accordingly. ax/ay are
 * unchanged from the original guess and still need the same real-hardware
 * check -- this mapping depends on how the board is physically held/
 * mounted, so don't assume they're right just because az turned out to
 * need flipping.
 */

/* Deviation from the gravity baseline (g) that starts a motion episode.
 * Raised from an initial 0.35g: that let casual handling/light taps enter
 * motion tracking at all, which combined with FUSION_MIN_MOTION_MS below
 * was still producing gesture events for a tap. */
#define FUSION_MOTION_ENTER_G          0.50f

/* Deviation from baseline (g) below which the board is considered at rest
 * again. Lower than the enter threshold on purpose (hysteresis) so a
 * motion episode doesn't flicker closed on a single noisy dip mid-motion.
 */
#define FUSION_MOTION_EXIT_G           0.15f

/* Consecutive "at rest" samples required (in addition to being below the
 * exit threshold) before a motion episode is closed out and classified.
 */
#define FUSION_REST_CONFIRM_SAMPLES    3

/* Minimum duration (ms) a motion episode must last before it's eligible to
 * be classified as a gesture at all. A tap/knock on the enclosure is a
 * sharp, near-instantaneous transient (typically settles back under
 * FUSION_MOTION_EXIT_G within a couple of samples); a deliberate swing/
 * shake/punch takes a real hand motion's worth of time. An episode that
 * closes out faster than this is discarded as noise (back to IDLE, no
 * event, no cooldown) rather than classified. */
#define FUSION_MIN_MOTION_MS           100

/* Hard cap on how long a single motion episode can run before it is
 * force-classified. Guards against a sensor that never settles back down.
 */
#define FUSION_MAX_MOTION_MS           800

/* Per-axis deviation magnitude (g) above which a sample counts toward
 * sign-reversal tracking (used to distinguish SHAKE from a directional
 * swing/punch). Raised alongside FUSION_MOTION_ENTER_G so reversal
 * tracking isn't more sensitive than motion entry itself. */
#define FUSION_REVERSAL_SIGN_G         0.30f

/* Minimum number of dominant-axis sign reversals within one motion episode
 * to classify it as GESTURE_SHAKE instead of a single directional gesture.
 * Raised from 3: a vigorous but single-direction swing/punch can pick up
 * a reversal or two from hand jitter alone, so 3 was firing on those too
 * often. */
#define FUSION_SHAKE_MIN_REVERSALS     4

/* Cooldown after emitting a gesture event before a new motion episode can
 * start. Keeps one physical motion from producing a flood of events. */
#define FUSION_GESTURE_COOLDOWN_MS     300

/* Peak deviation magnitude (g) that saturates reported gesture confidence
 * to 1.0. */
#define FUSION_CONFIDENCE_SATURATE_G   1.2f

/* Assumed resting accelerometer reading (g) on the "up" axis; used as the
 * baseline that motion deviation is measured against. */
#define FUSION_GRAVITY_G               1.0f

/* --- Continuous tilt_x (roll) axis --- */

/* Low-pass filter coefficient (exponential moving average), in (0,1].
 * Higher = less smoothing / more responsive. */
#define FUSION_TILT_LPF_ALPHA          0.15f

/* Publish period for the tilt_x EVT_AXIS event; throttled so the
 * downstream serial link isn't flooded (spec: ~20 Hz). */
#define FUSION_TILT_PUBLISH_MS         50

/* Roll angle (deg) that normalizes to a full-scale +-1.0 axis value. */
#define FUSION_TILT_MAX_DEG            45.0f
