plugins {
    kotlin("jvm") version "2.4.0" apply false
    kotlin("plugin.spring") version "2.4.0" apply false

    id("org.springframework.boot") version "4.1.0" apply false
}

allprojects {
    group = "com.rarmash.r4"
    version = "0.1.0-SNAPSHOT"

    repositories {
        mavenCentral()
    }
}