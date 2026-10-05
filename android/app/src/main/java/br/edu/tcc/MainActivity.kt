package br.edu.tcc

import android.app.Activity
import android.content.Intent
import android.os.Bundle
import android.os.Build
import android.text.InputType
import android.view.View
import android.view.WindowInsets
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ProgressBar
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import java.io.File
import java.util.concurrent.Executors

class MainActivity : Activity() {
    private lateinit var host: EditText
    private lateinit var repetitions: EditText
    private lateinit var start: Button
    private lateinit var cancel: Button
    private lateinit var export: Button
    private lateinit var status: TextView
    private lateinit var progress: ProgressBar
    private var exportSource: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        ExperimentController.restore(applicationContext)
        exportSource = savedInstanceState?.getString("exportSource")
        val padding = (20 * resources.displayMetrics.density).toInt()
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(padding, padding, padding, padding)
        }
        fun label(value: String) = TextView(this).apply {
            text = value
            textSize = 16f
            setPadding(0, padding / 2, 0, padding / 2)
            content.addView(this)
        }
        label("TCC • ${BuildConfig.CLIENT_NAME}").textSize = 24f
        label("Servidor IPv4 • TCP 62037")
        host = EditText(this).apply {
            id = View.generateViewId()
            hint = "Ex.: 192.168.1.10"
            contentDescription = "Endereço IPv4 do servidor"
            inputType = InputType.TYPE_CLASS_PHONE
            setSingleLine()
            setText(savedInstanceState?.getString("host") ?: ExperimentController.host)
            content.addView(this)
        }
        label("Repetições por combinação")
        repetitions = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            contentDescription = "Quantidade de repetições"
            setSingleLine()
            setText(savedInstanceState?.getString("repetitions") ?: ExperimentController.repetitions)
            content.addView(this)
        }
        label("Configure o servidor com a mesma quantidade. Serão executados 7 algoritmos × 3 tamanhos × N repetições. Mantenha o aplicativo aberto durante a coleta.")
        fun button(value: String, action: () -> Unit) = Button(this).apply {
            text = value
            setOnClickListener { action() }
            content.addView(this)
        }
        start = button("Iniciar experimento") { startExperiment() }
        cancel = button("Cancelar") { ExperimentController.cancel() }
        progress = ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal).apply {
            max = 1000
            content.addView(this)
        }
        status = label("").apply { setTextIsSelectable(true) }
        export = button("Exportar CSV") {
            val file = ExperimentController.state.csv ?: return@button
            exportSource = file.absolutePath
            startActivityForResult(Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "text/csv"
                putExtra(Intent.EXTRA_TITLE, file.name)
            }, EXPORT_CSV)
        }
        val scroll = ScrollView(this).apply { addView(content) }
        // API 36 enforces edge-to-edge; also accounts for the keyboard on API 29.
        scroll.setOnApplyWindowInsetsListener { view, insets ->
            if (Build.VERSION.SDK_INT >= 30) {
                val edges = insets.getInsets(WindowInsets.Type.systemBars() or
                    WindowInsets.Type.displayCutout() or WindowInsets.Type.ime())
                view.setPadding(edges.left, edges.top, edges.right, edges.bottom)
            } else {
                @Suppress("DEPRECATION")
                view.setPadding(insets.systemWindowInsetLeft, insets.systemWindowInsetTop,
                    insets.systemWindowInsetRight, insets.systemWindowInsetBottom)
            }
            insets
        }
        setContentView(scroll)
        scroll.requestApplyInsets()
    }

    override fun onStart() {
        super.onStart()
        ExperimentController.observer = ::render
        render(ExperimentController.state)
    }

    override fun onStop() {
        ExperimentController.observer = null
        super.onStop()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putString("host", host.text.toString())
        outState.putString("repetitions", repetitions.text.toString())
        outState.putString("exportSource", exportSource)
        super.onSaveInstanceState(outState)
    }

    private fun startExperiment() {
        val address = host.text.toString().trim()
        val parts = address.split('.')
        if (parts.size != 4 || parts.any {
                it.isEmpty() || it.any { char -> char !in '0'..'9' } ||
                    (it.length > 1 && it.startsWith('0')) || (it.toIntOrNull() ?: -1) !in 0..255
            } || address == "0.0.0.0" || address == "255.255.255.255") {
            host.error = "Informe o IPv4 do servidor, por exemplo 192.168.1.10."
            return
        }
        val count = repetitions.text.toString().toIntOrNull()
        if (count == null || count <= 0) {
            repetitions.error = "Informe um inteiro entre 1 e 2147483647."
            return
        }
        try {
            ExperimentController.start(applicationContext, address, count)
        } catch (error: Exception) {
            Toast.makeText(this, "Não foi possível iniciar: ${error.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun render(state: ExperimentController.State) {
        host.isEnabled = !state.running
        repetitions.isEnabled = !state.running
        start.isEnabled = state.ready && !state.running
        cancel.isEnabled = state.running && !state.cancelling
        export.isEnabled = !state.running && state.csv?.isFile == true
        progress.progress = if (state.total == 0L) 0 else (state.completed * 1000 / state.total).toInt()
        status.text = getString(R.string.experiment_status, state.completed, state.total,
            state.message, state.csv?.let { getString(R.string.csv_filename, it.name) } ?: "")
        if (state.running) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    @Deprecated("Platform callback used to retain API 29 compatibility")
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != EXPORT_CSV) return
        val path = exportSource
        exportSource = null
        if (resultCode != RESULT_OK || path == null) return
        val uri = data?.data ?: return
        val app = applicationContext
        exportWorker.execute {
            val message = try {
                val output = app.contentResolver.openOutputStream(uri, "wt")
                    ?: error("Destino indisponível")
                output.use { target -> File(path).inputStream().use { it.copyTo(target) } }
                "CSV exportado."
            } catch (error: Exception) { "Falha ao exportar: ${error.message}" }
            android.os.Handler(mainLooper).post { Toast.makeText(app, message, Toast.LENGTH_LONG).show() }
        }
    }

    companion object {
        private const val EXPORT_CSV = 1
        private val exportWorker = Executors.newSingleThreadExecutor()
    }
}
