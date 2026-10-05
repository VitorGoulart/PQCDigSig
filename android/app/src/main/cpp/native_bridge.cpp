#include <jni.h>

#include "tcc/client.hpp"
#include "tcc/crypto/runtime.hpp"

#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string string_value(JNIEnv* env, jstring value) {
    if (!value) throw std::invalid_argument("Missing string argument");
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) throw std::runtime_error("Could not read string");
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

void throw_java(JNIEnv* env, const std::exception& error) {
    if (env->ExceptionCheck()) return;
    auto type = env->FindClass("java/lang/IllegalStateException");
    if (type) env->ThrowNew(type, error.what());
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_br_edu_tcc_NativeBridge_create(JNIEnv* env, jobject) {
    try {
        return reinterpret_cast<jlong>(new tcc::client::Control());
    } catch (const std::exception& error) {
        throw_java(env, error);
        return 0;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_br_edu_tcc_NativeBridge_initializeCrypto(JNIEnv* env, jobject) {
    try {
        tcc::initialize_crypto_runtime();
    } catch (const std::exception& error) {
        throw_java(env, error);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_br_edu_tcc_NativeBridge_cancel(JNIEnv*, jobject, jlong handle) {
    if (handle) reinterpret_cast<tcc::client::Control*>(handle)->cancel();
}

extern "C" JNIEXPORT void JNICALL
Java_br_edu_tcc_NativeBridge_destroy(JNIEnv*, jobject, jlong handle) {
    delete reinterpret_cast<tcc::client::Control*>(handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_br_edu_tcc_NativeBridge_run(JNIEnv* env, jobject, jlong handle,
                               jstring host, jint repetitions, jstring client,
                               jstring output, jobject listener) {
    try {
        if (!handle || repetitions <= 0 || !listener)
            throw std::invalid_argument("Invalid run arguments");
        const auto type = env->GetObjectClass(listener);
        if (!type) return nullptr;
        const auto callback = env->GetMethodID(type, "onProgress", "(JJLjava/lang/String;)V");
        env->DeleteLocalRef(type);
        if (!callback) return nullptr;
        const tcc::client::Config config{
            string_value(env, host), static_cast<std::uint32_t>(repetitions),
            string_value(env, client), string_value(env, output)};
        const auto result = tcc::client::run(
            config, *reinterpret_cast<tcc::client::Control*>(handle),
            [&](const tcc::client::Progress& progress) {
                auto detail = env->NewStringUTF(progress.detail.c_str());
                if (!detail) throw std::runtime_error("Could not allocate progress string");
                env->CallVoidMethod(listener, callback, static_cast<jlong>(progress.completed),
                                    static_cast<jlong>(progress.total), detail);
                env->DeleteLocalRef(detail);
                if (env->ExceptionCheck()) throw std::runtime_error("Progress callback failed");
            });
        std::string message = result.cancelled ? "Cancelado." :
            (result.failed ? "Interrompido por falha." : "Concluído.");
        message += " Repetições válidas: " + std::to_string(result.succeeded) +
                   "; falhas: " + std::to_string(result.failed) + ".";
        if (!result.error.empty() && !result.cancelled) message += "\n" + result.error;
        if (result.cancelled || result.failed)
            message += "\nReinicie o servidor antes de um novo experimento.";
        return env->NewStringUTF(message.c_str());
    } catch (const std::exception& error) {
        throw_java(env, error);
        return nullptr;
    }
}
