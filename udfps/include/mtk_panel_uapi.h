#pragma once

#include <stdint.h>
#include <linux/ioctl.h>

typedef enum {
    PARAM_HBM = 0,
    PARAM_CABC,
    PARAM_ACL,
    PARAM_DC,
    POWER_MODE_MAX_NUM,
    PARAM_COLOR = POWER_MODE_MAX_NUM,
    PARAM_MAX_NUM
} paramId_t;

struct panel_param_info {
    paramId_t param_idx;
    uint32_t value;
};

// 'd' == DRM_IOCTL_BASE, 0x9e == DRM_COMMAND_END (0xa0) - 2
#define DRM_IOCTL_SET_PANEL_FEATURE _IOWR('d', 0x9e, struct panel_param_info)
