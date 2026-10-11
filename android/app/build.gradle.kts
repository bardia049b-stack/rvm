plugins {
    id("com.android.application")
}

android {
    namespace = "dev.rvm.app"
    compileSdk = 35

    externalNativeBuild {
        cmake {
            path = file("../jni/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    defaultConfig {
        applicationId = "dev.rvm.app"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=none",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                )
                cFlags += listOf(
                    "-std=c11", "-O2", "-fvisibility=hidden",
                    "-Wall", "-Wextra", "-Wno-unused-parameter",
                )
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
            signingConfig = signingConfigs.getByName("debug")
        }
        debug {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    packaging {
        resources.excludes += setOf(
            "META-INF/*.version", "META-INF/*.kotlin_module",
            "META-INF/LICENSE*", "META-INF/DEPENDENCIES",
            "Debug/*", "**/*.so.meta",
        )
        jniLibs.useLegacyPackaging = false
    }

    androidResources.noCompress += listOf("dtb")
}

dependencies {
}
