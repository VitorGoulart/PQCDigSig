#include <jni.h>

#include "tcc/crypto.hpp"

extern "C" JNIEXPORT jstring JNICALL
Java_br_edu_tcc_NativeBridge_nativeVersion(JNIEnv* env, jobject) {
    const bool adapters_linked =
        tcc::is_supported_algorithm(tcc::Algorithm::ml_dsa_44) &&
        tcc::is_supported_algorithm(tcc::Algorithm::falcon_512) &&
        tcc::is_supported_algorithm(tcc::Algorithm::ed25519);
    return env->NewStringUTF(
        adapters_linked ? "C++17 / API 29+ / crypto adapters linked"
                        : "C++17 / API 29+ / crypto adapters unavailable");
}
