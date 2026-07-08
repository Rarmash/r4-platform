plugins {
    kotlin("jvm")
    kotlin("plugin.spring")

    id("org.springframework.boot")
}

kotlin {
    jvmToolchain(21)
}

dependencies {
    implementation(
        platform(
            org.springframework.boot.gradle.plugin.SpringBootPlugin.BOM_COORDINATES
        )
    )

    implementation(project(":protocol"))

    implementation("org.springframework.boot:spring-boot-starter-webmvc")
    implementation("org.springframework.boot:spring-boot-starter-validation")
    implementation("org.springframework.boot:spring-boot-starter-actuator")

    implementation("org.jetbrains.kotlin:kotlin-reflect")
    implementation("tools.jackson.module:jackson-module-kotlin")

    testImplementation("org.springframework.boot:spring-boot-starter-test")
    testImplementation(kotlin("test"))

    testImplementation("org.springframework.boot:spring-boot-starter-test")
    testImplementation(kotlin("test"))
    testRuntimeOnly("org.junit.platform:junit-platform-launcher")

    implementation("org.springframework.boot:spring-boot-starter-jdbc")
    implementation("org.springframework.boot:spring-boot-starter-liquibase")

    runtimeOnly("org.postgresql:postgresql")

    testImplementation(
        "org.testcontainers:testcontainers-junit-jupiter:2.0.5"
    )
    testImplementation(
        "org.testcontainers:testcontainers-postgresql:2.0.5"
    )
}

tasks.withType<Test> {
    useJUnitPlatform()
}

tasks.bootJar {
    archiveFileName.set("r4-hub.jar")
}

tasks.jar {
    enabled = false
}
