plugins {
    id("com.android.application")
}

android {
    namespace = "br.edu.tcc"
    compileSdk = 36
    ndkVersion = "30.0.15729638"

    defaultConfig {
        applicationId = "br.edu.tcc.client"
        minSdk = 29
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        ndk {
            abiFilters += "arm64-v8a"
        }

        externalNativeBuild {
            cmake {
                cppFlags += "-std=c++17"
            }
        }
    }

    flavorDimensions += "client"
    productFlavors {
        create("clientAndroid16") {
            dimension = "client"
            applicationIdSuffix = ".android16"
            minSdk = 36
            buildConfigField("String", "CLIENT_NAME", "\"clientAndroid16\"")
        }
        create("clientAndroid10") {
            dimension = "client"
            applicationIdSuffix = ".android10"
            minSdk = 29
            buildConfigField("String", "CLIENT_NAME", "\"clientAndroid10\"")
        }
    }

    buildTypes {
        getByName("debug") {
            ndk {
                abiFilters += "x86_64"
            }
        }
    }

    buildFeatures {
        buildConfig = true
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
}
