package com.rarmash.r4.simulator

import org.springframework.boot.autoconfigure.SpringBootApplication
import org.springframework.boot.runApplication
import org.springframework.scheduling.annotation.EnableScheduling

@EnableScheduling
@SpringBootApplication
class R4SimulatorApplication

fun main(args: Array<String>) {
    runApplication<R4SimulatorApplication>(*args)
}