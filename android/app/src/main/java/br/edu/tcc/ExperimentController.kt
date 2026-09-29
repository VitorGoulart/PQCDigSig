package br.edu.tcc

import android.content.Context
import android.os.Handler
import android.os.Looper
import java.io.File
import java.util.UUID
import java.util.concurrent.Executors

/** Process-scoped worker: survives Activity recreation without retaining a screen. */
object ExperimentController {
    data class State(
        val running: Boolean = false,
        val cancelling: Boolean = false,
        val completed: Long = 0,
        val total: Long = 0,
        val message: String = "Pronto para iniciar.",
        val csv: File? = null,
    )

    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private val handleLock = Any()
    private var handle = 0L
    var state = State()
        private set
    var observer: ((State) -> Unit)? = null
    var host = ""
    var repetitions = "30"

    private fun publish(value: State) {
        state = value
        observer?.invoke(value)
    }

    fun restore(context: Context) {
        if (state.csv != null || state.running) return
        val preferences = context.getSharedPreferences("experiment", Context.MODE_PRIVATE)
        host = preferences.getString("host", "") ?: ""
        repetitions = preferences.getString("repetitions", "30") ?: "30"
        val name = preferences.getString("csv", null) ?: return
        val file = File(context.filesDir, "measurements/$name")
        if (file.isFile) publish(State(
            csv = file,
            message = "CSV anterior disponível. Uma execução interrompida pelo sistema não é retomada; reinicie o servidor antes de iniciar.",
        ))
    }

    fun start(context: Context, address: String, count: Int) {
        if (state.running) return
        val directory = File(context.filesDir, "measurements")
        check(directory.isDirectory || directory.mkdirs()) { "Não foi possível criar a pasta de resultados." }
        val file = File(directory, "${BuildConfig.CLIENT_NAME}-${UUID.randomUUID()}.csv")
        synchronized(handleLock) { handle = NativeBridge.create() }
        host = address
        repetitions = count.toString()
        context.getSharedPreferences("experiment", Context.MODE_PRIVATE).edit()
            .putString("host", host).putString("repetitions", repetitions)
            .putString("csv", file.name).apply()
        publish(State(running = true, total = 21L * count, message = "Iniciando…", csv = file))
        worker.execute {
            val message = try {
                NativeBridge.run(handle, address, count, BuildConfig.CLIENT_NAME, file.absolutePath,
                    object : NativeBridge.ProgressListener {
                        override fun onProgress(completed: Long, total: Long, detail: String) {
                            main.post {
                                publish(state.copy(completed = completed, total = total,
                                    message = if (state.cancelling) "Cancelando…" else detail))
                            }
                        }
                    })
            } catch (error: Exception) {
                "Falha: ${error.message}\nReinicie o servidor antes de tentar novamente."
            } finally {
                synchronized(handleLock) {
                    NativeBridge.destroy(handle)
                    handle = 0
                }
            }
            main.post { publish(state.copy(running = false, cancelling = false, message = message)) }
        }
    }

    fun cancel() {
        if (!state.running || state.cancelling) return
        synchronized(handleLock) { if (handle != 0L) NativeBridge.cancel(handle) }
        publish(state.copy(cancelling = true, message = "Cancelando após a operação criptográfica atual…"))
    }
}
