package com.fireball.mini.ui.viewmodels

import androidx.lifecycle.ViewModel
import com.fireball.mini.core.models.EgressMode
import com.fireball.mini.data.ShieldsRepository

class ShieldsViewModel(
    private val shieldsRepo: ShieldsRepository = ShieldsRepository()
) : ViewModel() {

    val stats = shieldsRepo.stats
    val egressStatus = shieldsRepo.egressStatus
    val sitePolicies = shieldsRepo.sitePolicies

    fun setEgressMode(mode: EgressMode) {
        shieldsRepo.setEgressMode(mode)
    }

    fun toggleShieldsForHost(hostname: String) {
        shieldsRepo.toggleShieldsForHost(hostname)
    }

    fun toggleScriptBlockingForHost(hostname: String) {
        shieldsRepo.toggleScriptBlockingForHost(hostname)
    }
}
