package com.fireball.mini.ui.viewmodels

import androidx.lifecycle.ViewModel
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.data.TransferRepository

class TransferViewModel(
    private val transferRepo: TransferRepository = TransferRepository()
) : ViewModel() {

    val discoveredMedia = transferRepo.discoveredMedia
    val transfers = transferRepo.transfers

    fun startDownload(media: DiscoveredMedia) {
        transferRepo.startTransfer(media)
    }

    fun pauseTransfer(transferId: String) {
        transferRepo.pauseTransfer(transferId)
    }

    fun resumeTransfer(transferId: String) {
        transferRepo.resumeTransfer(transferId)
    }

    fun cancelTransfer(transferId: String) {
        transferRepo.cancelTransfer(transferId)
    }

    fun removeTransfer(transferId: String) {
        transferRepo.removeTransfer(transferId)
    }
}
