package com.rarmash.r4.agent.linux.lifecycle

import java.time.Clock
import java.time.Duration
import java.time.Instant

class HubBackoff(
    private val clock: Clock = Clock.systemUTC(),
    private val initialDelay: Duration = Duration.ofSeconds(1),
    private val maxDelay: Duration = Duration.ofSeconds(30)
) {

    private var failureCount = 0
    private var nextAttemptAt: Instant = Instant.MIN

    fun canAttempt(): Boolean {
        return !Instant.now(clock).isBefore(nextAttemptAt)
    }

    fun recordSuccess() {
        failureCount = 0
        nextAttemptAt = Instant.MIN
    }

    fun recordFailure(): Duration {
        failureCount += 1

        val multiplier = 1L shl minOf(failureCount - 1, MAX_EXPONENT)
        val delay = initialDelay.multipliedBy(multiplier)
            .coerceAtMost(maxDelay)

        nextAttemptAt = Instant.now(clock).plus(delay)
        return delay
    }

    companion object {
        private const val MAX_EXPONENT = 20
    }
}
