package com.rarmash.r4.hub.support

import java.time.Clock
import java.time.Duration
import java.time.Instant
import java.time.ZoneId
import java.time.ZoneOffset

class MutableClock(
    private var currentInstant: Instant,
    private val currentZone: ZoneId = ZoneOffset.UTC
) : Clock() {

    override fun getZone(): ZoneId =
        currentZone

    override fun withZone(zone: ZoneId): Clock =
        MutableClock(currentInstant, zone)

    override fun instant(): Instant =
        currentInstant

    fun advance(duration: Duration) {
        currentInstant = currentInstant.plus(duration)
    }
}