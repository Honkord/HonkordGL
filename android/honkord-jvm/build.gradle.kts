plugins {
    kotlin("jvm") version "2.0.21"
    `java-library`
}

group = "org.honkord"
version = "0.1.0"

kotlin {
    jvmToolchain(17)
}

java {
    withSourcesJar()
}

val generateBindings by tasks.registering(Exec::class) {
    commandLine("python3", "${rootDir}/scripts/generate_bindings.py")
}

tasks.withType<org.jetbrains.kotlin.gradle.tasks.KotlinCompile>().configureEach {
    dependsOn(generateBindings)
}

tasks.test {
    useJUnitPlatform()
}