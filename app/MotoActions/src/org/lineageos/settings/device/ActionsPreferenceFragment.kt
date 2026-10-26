/*
 * SPDX-FileCopyrightText: 2015-2016 The CyanogenMod Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.device

import android.os.Bundle
import android.view.MenuItem
import androidx.preference.Preference
import com.android.settingslib.widget.SettingsBasePreferenceFragment
import org.lineageos.settings.device.SensorHelper.Companion.SENSOR_TYPE_MMI_CAMERA_ACTIVATE

class ActionsPreferenceFragment : SettingsBasePreferenceFragment() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.actions_panel, rootKey)
        findPreference<Preference>(MotoActionsSettings.GESTURE_TWIST_KEY)?.isVisible =
            SensorHelper(requireContext()).getSensor(SENSOR_TYPE_MMI_CAMERA_ACTIVATE) != null
    }

    @Deprecated("Deprecated in Java")
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return if (item.itemId == android.R.id.home) {
            activity?.onBackPressedDispatcher?.onBackPressed()
            true
        } else {
            false
        }
    }
}
