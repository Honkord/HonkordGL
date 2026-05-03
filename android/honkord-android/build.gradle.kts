plugins {
    id("com.android.library") version "8.5.2"
    id("org.jetbrains.kotlin.android") version "2.0.21"
}

group = "org.honkord"
version = "0.1.0"

android {
    namespace = "org.honkord.android"
    compileSdk = 34

    defaultConfig {
        minSdk = 24
        consumerProguardFiles("consumer-rules.pro")

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17")
                arguments += listOf(
                    "-DHONKORDGL_LIB_DIR=${rootDir.absolutePath}/../build/android/native-libs/${'$'}{ANDROID_ABI}"
                )
            }
        }

        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../native-bridge/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    sourceSets {
        getByName("main") {
            java.srcDirs("src/main/kotlin")
        }
    }
}

val generateBindings by tasks.registering(Exec::class) {
    commandLine("python3", "${rootDir}/scripts/generate_bindings.py")
}

tasks.matching { it.name.contains("externalNativeBuild", ignoreCase = true) }.configureEach {
    dependsOn(generateBindings)
}

dependencies {
    implementation(kotlin("stdlib"))
}
