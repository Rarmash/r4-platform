package com.rarmash.r4.agent.linux.command

import com.fasterxml.jackson.module.kotlin.readValue
import com.rarmash.r4.agent.linux.hub.HttpHubClient
import com.rarmash.r4.agent.linux.system.SystemInfo
import com.rarmash.r4.agent.linux.system.SystemInfoCollector
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CommandStatus
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue
import java.time.Instant
import java.util.UUID

class SystemInfoCommandHandlerTest {

    @Test
    fun `returns system info as json`() {
        val handler = SystemInfoCommandHandler(
            systemInfoCollector = SystemInfoCollector {
                SystemInfo(
                    hostname = "host-1",
                    osName = "Test Linux",
                    osVersion = "1",
                    kernelVersion = "6.0",
                    architecture = "amd64",
                    availableProcessors = 4,
                    totalMemoryBytes = 1024,
                    uptimeSeconds = 55,
                    agentVersion = "0.1.0"
                )
            }
        )

        val result = handler.handle(command())
        val body = HttpHubClient.defaultObjectMapper()
            .readValue<Map<String, Any?>>(result.result!!)

        assertTrue(result.success)
        assertEquals("host-1", body["hostname"])
        assertEquals("Test Linux", body["osName"])
        assertEquals("0.1.0", body["agentVersion"])
    }

    private fun command(): CommandResponse {
        return CommandResponse(
            id = UUID.randomUUID(),
            deviceId = UUID.randomUUID(),
            type = "system.info",
            parameters = emptyMap(),
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
