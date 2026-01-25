// Check Java version compatibility
val javaVersion = System.getProperty("java.version") ?: "unknown"
val javaMajorVersion = javaVersion.split(".")[0].toIntOrNull() ?: 0

if (javaMajorVersion > 17) {
    logger.warn("⚠️  Java version $javaVersion detected (major: $javaMajorVersion)")
    logger.warn("    Kotlin compiler may have issues with Java versions > 17")
    logger.warn("    Recommended: Use Java 17 for this build")
}

// Check if we should use prebuilt native libraries
val usePrebuiltNativeLibs = project.findProperty("rpcsx.usePrebuiltNativeLibs")?.toString()?.toBoolean() ?: false

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.compose.compiler)
    id("org.jetbrains.kotlin.plugin.serialization")
    id("kotlin-parcelize")
}

android {
    namespace = "net.rpcsx"
    compileSdk = 36
    ndkVersion = "29.0.13113456"

    defaultConfig {
        applicationId = "net.rpcsx"
        minSdk = 29
        targetSdk = 35
        versionCode = 30018
        versionName = "${System.getenv("RX_VERSION") ?: "1.5.0-neon"}${if (System.getenv("RX_SHA") != null) "-" + System.getenv("RX_SHA") else ""}"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        ndk {
            abiFilters += listOf("arm64-v8a")  // Оптимізовано тільки для ARMv9
        }

        buildConfigField("String", "Version", "\"v${versionName}\"")
        buildConfigField("String", "OptimizationTarget", "\"Snapdragon 8s Gen 3 (ARMv9-A + NEON)\"")
        buildConfigField("boolean", "EnableARMv9Optimizations", "true")
        buildConfigField("boolean", "EnableNEONIntrinsics", "true")
    }

    signingConfigs {
        create("custom-key") {
            val keystoreAlias = System.getenv("KEYSTORE_ALIAS") ?: ""
            val keystorePassword = System.getenv("KEYSTORE_PASSWORD") ?: ""
            val keystorePath = System.getenv("KEYSTORE_PATH") ?: ""

            if (keystorePath.isNotEmpty() && file(keystorePath).exists() && file(keystorePath).length() > 0) {
                keyAlias = keystoreAlias
                keyPassword = keystorePassword
                storeFile = file(keystorePath)
                storePassword = keystorePassword
            } else {
                val debugKeystoreFile = file("${System.getProperty("user.home")}/debug.keystore")

                println("⚠️ Custom keystore not found or empty! creating debug keystore.")

                if (!debugKeystoreFile.exists()) {
                    Runtime.getRuntime().exec(
                        arrayOf(
                            "keytool", "-genkeypair",
                            "-v", "-keystore", debugKeystoreFile.absolutePath,
                            "-storepass", "android",
                            "-keypass", "android",
                            "-alias", "androiddebugkey",
                            "-keyalg", "RSA",
                            "-keysize", "2048",
                            "-validity", "10000",
                            "-dname", "CN=Android Debug,O=Android,C=US"
                        )
                    ).waitFor()
                }

                keyAlias = "androiddebugkey"
                keyPassword = "android"
                storeFile = debugKeystoreFile
                storePassword = "android"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            signingConfig = signingConfigs.getByName("custom-key") ?: signingConfigs.getByName("debug")
            
            // Optimize native compilation with full debug symbols
            ndk {
                debugSymbolLevel = "full"
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    // Only use CMake build if not using prebuilt native libraries
    if (!usePrebuiltNativeLibs) {
        externalNativeBuild {
            cmake {
                path = file("src/main/cpp/CMakeLists.txt")
                version = "3.22.1+"  // Minimum CMake version, allows newer
            }
        }
    } else {
        logger.lifecycle("📦 Using prebuilt native libraries from jniLibs/")
    }

    // Configure jniLibs source set for prebuilt libraries
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }

    buildFeatures {
        viewBinding = true
        compose = true
        buildConfig = true
    }

    composeOptions {
        kotlinCompilerExtensionVersion = "1.5.15"
    }

    packaging {
        // This is necessary for libadrenotools custom driver loading
        jniLibs.useLegacyPackaging = true
    }
}

base.archivesName = "rpcsx"

dependencies {
    implementation(libs.androidx.navigation.compose)
    implementation(libs.androidx.ui.tooling.preview.android)
    val composeBom = platform("androidx.compose:compose-bom:2025.02.00")
    implementation(composeBom)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.appcompat)
    implementation(libs.material)
    implementation(libs.androidx.constraintlayout)
    implementation(libs.androidx.activity)
    testImplementation(libs.junit)
    androidTestImplementation(libs.androidx.junit)
    androidTestImplementation(libs.androidx.espresso.core)
    debugImplementation(libs.androidx.ui.tooling)
    implementation(libs.kotlinx.serialization.json)
    implementation(libs.coil.compose)
    implementation(libs.squareup.okhttp3)
}
// Build trigger Fri Jan 23 16:04:08 UTC 2026
