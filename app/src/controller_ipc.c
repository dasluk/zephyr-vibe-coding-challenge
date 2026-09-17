#include "controller_ipc.h"

K_MSGQ_DEFINE(imu_sample_q, sizeof(struct imu_sample), 16, 4);
K_MSGQ_DEFINE(button_evt_q, sizeof(struct button_event), 8, 4);
K_MSGQ_DEFINE(controller_evt_q, sizeof(struct controller_event), 16, 4);
