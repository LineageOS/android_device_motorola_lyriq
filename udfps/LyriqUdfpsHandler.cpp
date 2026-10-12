/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "libudfpshandler"

#include "UdfpsHandler.h"
#include "mtk_panel_uapi.h"

#include <android-base/logging.h>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

enum HBM_STATE { OFF = 0, ON = 2 };

class LyriqUdfpsHandler : public UdfpsHandler {
  public:
    LyriqUdfpsHandler() : mCard0Fd(-1) {}

    ~LyriqUdfpsHandler() override {
        if (mCard0Fd >= 0) {
            close(mCard0Fd);
            mCard0Fd = -1;
        }
    }

    void init(fingerprint_device_t* device) override {
        mDevice = device;

        LOG(INFO) << "Lyriq UDFPS handler initialized";
    }

    void onFingerDown(uint32_t x, uint32_t y, float minor, float major) override {
        if (mFingerDown.exchange(true)) return;

        LOG(INFO) << "Finger down: x=" << x << " y=" << y;

        setHbm(true);

        sendGoodixExtCmd(241, x, y, minor, major);
    }

    void onFingerUp() override {
        if (!mFingerDown.exchange(false)) return;

        LOG(INFO) << "Finger up";

        sendGoodixExtCmd(242, 0, 0, 0, 0);

        setHbm(false);
    }

    void onAcquired(int32_t result, int32_t vendorCode) override {
        LOG(DEBUG) << "onAcquired: result=" << result << " vendorCode=" << vendorCode;
    }

    void onAuthenticationSucceeded() override {
        LOG(INFO) << "onAuthenticationSucceeded";
        mFingerDown.store(false);
        setHbm(false);
    }

    void onAuthenticationFailed() override {
        LOG(INFO) << "onAuthenticationFailed";
        mFingerDown.store(false);
        setHbm(false);
    }

    void cancel() override {
        LOG(INFO) << "UDFPS cancel";
        mFingerDown.store(false);
        setHbm(false);
    }

  private:
    fingerprint_device_t* mDevice = nullptr;
    std::atomic<bool> mHbmActive{false};
    std::atomic<bool> mFingerDown{false};
    int mCard0Fd = -1;

    int sendGoodixExtCmd(int32_t cmd, uint32_t x, uint32_t y, float minor, float major) {
        if (!mDevice) {
            LOG(ERROR) << "mDevice is null, cannot send extCmd";
            return -1;
        }

        void** dev_vtable = reinterpret_cast<void**>(mDevice);
        auto extCmd = reinterpret_cast<int (*)(fingerprint_device_t*, int32_t, uint32_t, uint32_t, float, float)>(dev_vtable[26]);
        if (extCmd) {
            LOG(DEBUG) << "Sending Goodix extCmd (vtable 26): cmd=" << cmd << " x=" << x << " y=" << y;
            return extCmd(mDevice, cmd, x, y, minor, major);
        }

        LOG(ERROR) << "Failed to resolve Goodix extCmd pointer";
        return -1;
    }

    void setHbm(bool enable) {
        if (mHbmActive.load() == enable) {
            LOG(DEBUG) << "setHbm: Already " << (enable ? "ON" : "OFF") << ", skipping";
            return;
        }

        LOG(INFO) << "setHbm: " << (enable ? "ON" : "OFF");

        if (mCard0Fd < 0) {
            mCard0Fd = open("/dev/dri/card0", O_RDWR);
            if (mCard0Fd < 0) {
                LOG(ERROR) << "Failed to open /dev/dri/card0: " << strerror(errno);
                return;
            }
        }

        struct panel_param_info param_info;
        param_info.param_idx = PARAM_HBM;
        param_info.value = enable ? ON : OFF;

        int32_t ret = ioctl(mCard0Fd, DRM_IOCTL_SET_PANEL_FEATURE, &param_info);
        if (ret < 0) {
            LOG(ERROR) << "IOCTL DRM_IOCTL_SET_PANEL_FEATURE failed with ret = " << ret
                       << ", errno = " << errno << " (" << strerror(errno) << ")";
            if (errno == EBADF) {
                close(mCard0Fd);
                mCard0Fd = -1;
            }
        } else {
            mHbmActive.store(enable);
            LOG(INFO) << "HBM state set successfully. New state: " << (enable ? ON : OFF);
        }
    }
};

static UdfpsHandler* create_handler() {
    return new LyriqUdfpsHandler();
}

static void destroy_handler(UdfpsHandler* handler) {
    delete handler;
}

extern "C" UdfpsHandlerFactory UDFPS_HANDLER_FACTORY = {
    .create = create_handler,
    .destroy = destroy_handler,
};
