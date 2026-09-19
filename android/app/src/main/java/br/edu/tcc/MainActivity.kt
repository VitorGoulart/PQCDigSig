package br.edu.tcc

import android.app.Activity
import android.os.Bundle
import android.widget.TextView

class MainActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val status = TextView(this).apply {
            text = getString(
                R.string.status_text,
                BuildConfig.CLIENT_NAME,
                NativeBridge.nativeVersion(),
            )
            textSize = 18f
            setPadding(32, 64, 32, 32)
        }
        setContentView(status)
    }
}
