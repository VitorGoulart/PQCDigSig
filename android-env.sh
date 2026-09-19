#!/usr/bin/env bash

# Use com: source android-env.sh
export ANDROID_HOME="${ANDROID_HOME:-/home/vitor/Android/Sdk}"
export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME}}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-${ANDROID_HOME}/ndk/30.0.15729638}"
export JAVA_HOME="${JAVA_HOME:-/opt/android-studio/jbr}"
export PATH="${ANDROID_HOME}/platform-tools:${ANDROID_HOME}/cmdline-tools/latest/bin:${JAVA_HOME}/bin:${PATH}"
