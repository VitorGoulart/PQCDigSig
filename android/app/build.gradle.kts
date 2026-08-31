plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "br.edu.tcc"
    compileSdk = 37

    defaultConfig {
        applicationId = "br.edu.tcc"
        minSdk = 26
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17")
                arguments += listOf(
                    "-DTCC_LIBOQS_ANDROID_ROOT=${rootProject.projectDir}/../third_party/install/liboqs-android",
                    "-DTCC_OPENSSL_ANDROID_ROOT=${rootProject.projectDir}/../third_party/install/openssl-android"
                )
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "4.1.2"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }
}
