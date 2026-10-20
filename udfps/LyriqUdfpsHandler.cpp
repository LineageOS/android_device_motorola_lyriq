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
#include <mutex>
#include <xf86drmMode.h>
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
        std::lock_guard<std::mutex> lock(mMutex);
        if (mFingerDown) return;

        LOG(INFO) << "Finger down: x=" << x << " y=" << y;
        mFingerDown = true;
        mTouch = {x, y, minor, major};
    }

    void onUiReady() override {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFingerDown || mScanActive) return;

        if (!waitFor144Hz()) {
            LOG(ERROR) << "UDFPS display did not reach 144 Hz; skipping capture";
            return;
        }

        LOG(INFO) << "UDFPS UI ready at 144 Hz; enabling HBM and starting capture";
        setHbm(true);
        if (!mHbmActive.load()) {
            LOG(ERROR) << "HBM did not enable; skipping fingerprint capture";
            return;
        }

        sendGoodixExtCmd(241, mTouch.x, mTouch.y, mTouch.minor, mTouch.major);
        mScanActive = true;
    }

    void onFingerUp() override {
        std::lock_guard<std::mutex> lock(mMutex);
        if (!mFingerDown) return;

        LOG(INFO) << "Finger up";
        mFingerDown = false;
        if (mScanActive) {
            sendGoodixExtCmd(242, 0, 0, 0, 0);
            mScanActive = false;
        }
        setHbm(false);
    }

    void onAcquired(int32_t result, int32_t vendorCode) override {
        LOG(DEBUG) << "onAcquired: result=" << result << " vendorCode=" << vendorCode;
    }

    void onAuthenticationSucceeded() override {
        std::lock_guard<std::mutex> lock(mMutex);
        LOG(INFO) << "onAuthenticationSucceeded";
        mFingerDown = false;
        mScanActive = false;
        setHbm(false);
    }

    void onAuthenticationFailed() override {
        std::lock_guard<std::mutex> lock(mMutex);
        LOG(INFO) << "onAuthenticationFailed";
        mFingerDown = false;
        mScanActive = false;
        setHbm(false);
    }

    void cancel() override {
        std::lock_guard<std::mutex> lock(mMutex);
        LOG(INFO) << "UDFPS cancel";
        mFingerDown = false;
        mScanActive = false;
        setHbm(false);
    }

  private:
    struct Touch {
        uint32_t x;
        uint32_t y;
        float minor;
        float major;
    };

    fingerprint_device_t* mDevice = nullptr;
    std::mutex mMutex;
    std::atomic<bool> mHbmActive{false};
    bool mFingerDown = false;
    bool mScanActive = false;
    Touch mTouch{};
    int mCard0Fd = -1;

    // onUiReady follows the 144 Hz request, but can precede the hardware switch.
    bool waitFor144Hz() {
        if (mCard0Fd < 0) {
            mCard0Fd = open("/dev/dri/card0", O_RDWR);
            if (mCard0Fd < 0) {
                LOG(ERROR) << "Failed to open /dev/dri/card0: " << strerror(errno);
                return false;
            }
        }

        for (int attempt = 0; attempt < 75; ++attempt) {
            bool at144Hz = false;
            drmModeResPtr resources = drmModeGetResources(mCard0Fd);
            if (resources) {
                for (int i = 0; i < resources->count_crtcs; ++i) {
                    drmModeCrtcPtr crtc = drmModeGetCrtc(mCard0Fd, resources->crtcs[i]);
                    if (crtc) {
                        // Lyriq's built-in display is 1080 x 2400.
                        at144Hz = crtc->mode_valid && crtc->mode.hdisplay == 1080 &&
                                  crtc->mode.vdisplay == 2400 &&
                                  crtc->mode.vrefresh >= 143 && crtc->mode.vrefresh <= 145;
                        drmModeFreeCrtc(crtc);
                    }
                    if (at144Hz) break;
                }
                drmModeFreeResources(resources);
            }
            if (at144Hz) {
                usleep(8000); // One 144 Hz frame for the panel to settle.
                return true;
            }
            usleep(2000);
        }
        return false;
    }

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
