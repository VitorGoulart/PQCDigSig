package br.edu.tcc

object NativeBridge {
    init { System.loadLibrary("tcc_android") }

    interface ProgressListener {
        fun onProgress(completed: Long, total: Long, detail: String)
    }

    external fun create(): Long
    external fun cancel(handle: Long)
    external fun destroy(handle: Long)
    external fun run(
        handle: Long, host: String, repetitions: Int, client: String,
        output: String, listener: ProgressListener,
    ): String
}
