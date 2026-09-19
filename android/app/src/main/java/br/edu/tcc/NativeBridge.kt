package br.edu.tcc

object NativeBridge {
    init {
        System.loadLibrary("tcc_android")
    }

    external fun nativeVersion(): String
}
