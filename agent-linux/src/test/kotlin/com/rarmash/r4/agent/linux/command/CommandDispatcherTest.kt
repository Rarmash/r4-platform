package com.rarmash.r4.agent.linux.command

import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CommandStatus
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue
import java.time.Instant
import java.util.UUID

class CommandDispatcherTest {

    @Test
    fun `dispatches echo command`() {
        val dispatcher = CommandDispatcher(listOf(EchoCommandHandler()))

        val result = dispatcher.dispatch(
            command(type = "command.echo", parameters = mapOf("message" to "hello"))
        )

        assertTrue(result.success)
        assertEquals("hello", result.result)
    }

    @Test
    fun `reports unsupported command`() {
        val dispatcher = CommandDispatcher(listOf(EchoCommandHandler()))

        val result = dispatcher.dispatch(command(type = "unknown.command"))

        assertFalse(result.success)
        assertEquals("Unsupported command type: unknown.command", result.error)
    }

    private fun command(
        type: String,
        parameters: Map<String, String> = emptyMap()
    ): CommandResponse {
        return CommandResponse(
            id = UUID.randomUUID(),
            deviceId = UUID.randomUUID(),
            type = type,
            parameters = parameters,
            status = CommandStatus.RUNNING,
            result = null,
            error = null,
            attemptCount = 1,
            leaseExpiresAt = Instant.now().plusSeconds(30),
            leaseToken = UUID.randomUUID(),
            createdAt = Instant.now(),
            startedAt = Instant.now(),
            completedAt = null
        )
    }
}
