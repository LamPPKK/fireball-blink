package com.fireball.mini

import android.app.Application

class FireballApp : Application() {
    companion object {
        var instance: FireballApp? = null
            private set
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
    }
}
