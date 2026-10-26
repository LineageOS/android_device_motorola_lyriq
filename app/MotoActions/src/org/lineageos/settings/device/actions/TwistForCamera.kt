/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.device.actions

import android.app.ActivityManager
import android.app.KeyguardManager
import android.app.admin.DevicePolicyManager
import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.os.PowerManager
import android.os.SystemClock
import android.os.UserHandle
import android.os.UserManager
import android.provider.MediaStore
import android.provider.Settings
import android.util.Log
import org.lineageos.settings.device.MotoActionsSettings.GESTURE_TWIST_KEY
import org.lineageos.settings.device.SensorHelper
import org.lineageos.settings.device.SensorHelper.Companion.SENSOR_TYPE_MMI_CAMERA_ACTIVATE

class TwistForCamera(
    private val context: Context,
    private val sharedPreferences: SharedPreferences,
    private val sensorHelper: SensorHelper,
) : UpdatedStateNotifier, SensorEventListener {

    private val twistSensor = sensorHelper.getSensor(SENSOR_TYPE_MMI_CAMERA_ACTIVATE)
    private val proximitySensor = sensorHelper.getSensor(Sensor.TYPE_PROXIMITY)
    private val powerManager = context.getSystemService(PowerManager::class.java)
    private var isEnabled = false
    private var proxIsCovered = proximitySensor != null
    private var lastLaunchTime = -LAUNCH_COOLDOWN_MS

    override fun updateState() {
        val enabled = twistSensor != null && sharedPreferences.getBoolean(GESTURE_TWIST_KEY, false)
        if (enabled == isEnabled) return
        if (enabled) {
            proxIsCovered = proximitySensor != null
            if (
                proximitySensor != null &&
                !sensorHelper.registerListener(proximitySensor, this)
            ) {
                Log.e(TAG, "Cannot monitor proximity; leaving twist disabled")
                return
            }
            isEnabled = sensorHelper.registerListener(twistSensor!!, this)
            if (!isEnabled) sensorHelper.unregisterListener(this)
        } else {
            stop()
        }
    }

    fun stop() {
        sensorHelper.unregisterListener(this)
        isEnabled = false
    }

    override fun onSensorChanged(event: SensorEvent) {
        if (!isEnabled) return
        if (event.sensor.type == Sensor.TYPE_PROXIMITY) {
            proxIsCovered = event.values[0] < event.sensor.maximumRange
            return
        }
        if (event.sensor.type != SENSOR_TYPE_MMI_CAMERA_ACTIVATE || proxIsCovered) return

        val now = SystemClock.elapsedRealtime()
        if (now - lastLaunchTime < LAUNCH_COOLDOWN_MS) return

        // The service runs as the system user; launch for the person using the phone.
        val user = UserHandle.of(ActivityManager.getCurrentUser())
        val userContext = context.createContextAsUser(user, 0)
        if (
            Settings.Secure.getInt(
                userContext.contentResolver, Settings.Secure.USER_SETUP_COMPLETE, 0
            ) == 0
        ) {
            return
        }
        val policyManager = userContext.getSystemService(DevicePolicyManager::class.java)
        if (
            userContext.getSystemService(UserManager::class.java)
                .hasUserRestriction(UserManager.DISALLOW_CAMERA) ||
                policyManager.getCameraDisabled(null)
        ) {
            return
        }

        // Always use the secure camera while locked, without falling back to the normal camera.
        val keyguardManager = userContext.getSystemService(KeyguardManager::class.java)
        val action = if (keyguardManager.isKeyguardLocked || keyguardManager.isDeviceLocked) {
            if (
                policyManager.getKeyguardDisabledFeatures(null) and
                    DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA != 0
            ) {
                return
            }
            MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA_SECURE
        } else {
            MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA
        }
        val intent = Intent(action).addFlags(
            Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
        )
        try {
            powerManager.wakeUp(SystemClock.uptimeMillis(), PowerManager.WAKE_REASON_GESTURE, TAG)
            context.startActivityAsUser(intent, user)
            lastLaunchTime = now
        } catch (e: ActivityNotFoundException) {
            Log.w(TAG, "No camera handles $action", e)
        } catch (e: SecurityException) {
            Log.w(TAG, "Camera launch was denied", e)
        }
    }

    override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {}

    companion object {
        private const val TAG = "TwistForCamera"
        private const val LAUNCH_COOLDOWN_MS = 1500L
    }
}
