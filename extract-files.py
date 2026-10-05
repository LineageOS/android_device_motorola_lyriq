#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)

from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
)

from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/motorola/lyriq',
    'hardware/mediatek',
    'hardware/motorola',
]


def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None

blob_fixups: blob_fixups_user_type = {
    'vendor/bin/hw/android.hardware.audio.service-aidl.mediatek': blob_fixup()
        .replace_needed('android.media.audio.common.types-V5-ndk.so', 'android.media.audio.common.types-V3-ndk.so')
        .replace_needed('libaudio_aidl_conversion_common_ndk.so', 'libaudio_aidl_conversion_common_ndk_prebuilt.so'),
    'vendor/lib64/hw/android.hardware.audio.effect.aidl-impl-mediatek.so': blob_fixup()
        .replace_needed('android.media.audio.common.types-V5-ndk.so', 'android.media.audio.common.types-V3-ndk.so')
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),
    'vendor/lib64/android.hardware.audio.core-impl-mediatek.so': blob_fixup()
        .add_needed('libaudioutils_shim.so')
        .replace_needed('android.media.audio.common.types-V5-ndk.so', 'android.media.audio.common.types-V3-ndk.so')
        .replace_needed('libaudio_aidl_conversion_common_ndk.so', 'libaudio_aidl_conversion_common_ndk_prebuilt.so'),
    'vendor/lib64/libaudio_aidl_conversion_common_ndk_prebuilt.so': blob_fixup()
        .replace_needed('android.media.audio.common.types-V5-ndk.so', 'android.media.audio.common.types-V3-ndk.so'),
    'vendor/lib64/hw/android.hardware.soundtrigger3-impl.so': blob_fixup()
        .replace_needed('android.media.audio.common.types-V5-ndk.so', 'android.media.audio.common.types-V3-ndk.so')
        .replace_needed('libaudio_aidl_conversion_common_ndk.so', 'libaudio_aidl_conversion_common_ndk_prebuilt.so'),
    'vendor/lib64/hw/audio.primary.mt6893.so': blob_fixup()
        .replace_needed('libtinyxml2.so', 'libtinyxml2-v34.so'),
    ('vendor/lib64/vendor.mediatek.hardware.camera.isphal-V1-ndk.so', 'vendor/lib64/libmtkcam_grallocutils.so'): blob_fixup()
        .replace_needed('android.hardware.graphics.common-V5-ndk.so', 'android.hardware.graphics.common-V7-ndk.so'),
    'vendor/lib64/libmtkcam_hal_aidl_common.so': blob_fixup()
        .replace_needed('android.hardware.camera.common-V2-ndk.so', 'android.hardware.camera.common-V1-ndk.so'),
    'vendor/lib64/libcam.utils.sensorprovider.so': blob_fixup()
        .replace_needed('android.hardware.sensors-V2-ndk.so', 'android.hardware.sensors-V3-ndk.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    'lyriq',
    'motorola',
    blob_fixups=blob_fixups,
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
)

if __name__ == '__main__':
    utils = ExtractUtils.device(module)
    utils.run()
