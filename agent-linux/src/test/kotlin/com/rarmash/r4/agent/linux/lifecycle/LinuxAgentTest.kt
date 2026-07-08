package com.rarmash.r4.agent.linux.lifecycle

import com.rarmash.r4.agent.linux.command.CommandDispatcher
import com.rarmash.r4.agent.linux.command.EchoCommandHandler
import com.rarmash.r4.agent.linux.command.SystemInfoCommandHandler
import com.rarmash.r4.agent.linux.config.AgentConfiguration
import com.rarmash.r4.agent.linux.hub.HubClient
import com.rarmash.r4.agent.linux.identity.AgentIdentity
import com.rarmash.r4.agent.linux.system.SystemInfo
import com.rarmash.r4.agent.linux.system.SystemInfoCollector
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CommandStatus
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.DeviceStatus
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue
import java.net.URI
import java.nio.file.Path
import java.time.Clock
import java.time.Duration
import java.time.Instant
import java.time.ZoneId
import java.util.UUID

class LinuxAgentTest {

    @Test
    fun `registers with command capabilities`() {
        val hub = RecordingHubClient()
        val agent = linuxAgent(hub)

        agent.registerIfNeeded()

        assertEquals(setOf("command.echo", "system.info"), hub.registration?.capabilities)
    }

    @Test
    fun `executes echo and completes with lease token`() {
        val command = command(
            type = "command.echo",
            parameters = mapOf("message" to "pong")
        )
        val hub = RecordingHubClient(nextCommand = command)
        val agent = linuxAgent(hub)

        agent.pollCommandSafely()

        assertEquals(command.leaseToken, hub.completedLeaseToken)
        assertEquals("pong", hub.completedResult)
        assertEquals(true, hub.completedSuccess)
    }

    @Test
    fun `temporary fetch failure does not crash polling`() {
        val hub = RecordingHubClient(fetchFailure = RuntimeException("hub unavailable"))
        val agent = linuxAgent(hub)

        agent.pollCommandSafely()

        assertNotNull(hub.registration)
        assertEquals(null, hub.completedSuccess)
    }

    @Test
    fun `backoff skips immediate retry after temporary Hub failure`() {
        val clock = MutableTestClock()
        val hub = RecordingHubClient(fetchFailure = RuntimeException("hub unavailable"))
        val agent = linuxAgent(
            hubClient = hub,
            hubBackoff = HubBackoff(clock = clock)
        )

        agent.pollCommandSafely()
        agent.pollCommandSafely()

        assertEquals(1, hub.fetchCount)

        clock.advance(Duration.ofSeconds(1))
        agent.pollCommandSafely()

        assertEquals(2, hub.fetchCount)
    }

    @Test
    fun `sends heartbeat through Hub client`() {
        val hub = RecordingHubClient()
        val agent = linuxAgent(hub)

        agent.heartbeatSafely()

        assertEquals(1, hub.heartbeatCount)
    }

    @Test
    fun `executes system info command`() {
        val command = command(type = "system.info")
        val hub = RecordingHubClient(nextCommand = command)
        val agent = linuxAgent(hub)

        agent.pollCommandSafely()

        assertEquals(true, hub.completedSuccess)
        assertTrue(hub.completedResult?.contains("\"hostname\":\"host\"") == true)
    }

    @Test
    fun `completes unsupported command with error`() {
        val command = command(type = "unknown.command")
        val hub = RecordingHubClient(nextCommand = command)
        val agent = linuxAgent(hub)

        agent.pollCommandSafely()

        assertEquals(false, hub.completedSuccess)
        assertEquals("Unsupported command type: unknown.command", hub.completedError)
    }

    @Test
    fun `does not complete same command twice after successful completion`() {
        val command = command(
            type = "command.echo",
            parameters = mapOf("message" to "once")
        )
        val hub = RecordingHubClient(nextCommand = command)
        val agent = linuxAgent(hub)

        agent.pollCommandSafely()
        agent.pollCommandSafely()

        assertEquals(1, hub.completeCount)
    }

    @Test
    fun `stops scheduler lifecycle`() {
        val agent = linuxAgent(RecordingHubClient())

        agent.stop()

        // Повторная остановка не должна падать: это важно для shutdown hook и тестовых запусков.
        agent.stop()
    }

    private fun linuxAgent(
        hubClient: RecordingHubClient,
        hubBackoff: HubBackoff = HubBackoff()
    ): LinuxAgent {
        return LinuxAgent(
            configuration = AgentConfiguration(
                hubUrl = URI.create("http://localhost:8080"),
                agentName = "test-agent",
                dataDir = Path.of("build/test-agent"),
                heartbeatInterval = Duration.ofSeconds(10),
                commandPollInterval = Duration.ofSeconds(5),
                httpConnectTimeout = Duration.ofSeconds(1),
                httpRequestTimeout = Duration.ofSeconds(1)
            ),
            identity = object : AgentIdentity {
                override val agentId: UUID = FIXED_AGENT_ID
            },
            hubClient = hubClient,
            hubBackoff = hubBackoff,
            commandDispatcher = CommandDispatcher(
                listOf(
                    EchoCommandHandler(),
                    SystemInfoCommandHandler(
                        SystemInfoCollector {
                            SystemInfo(
                                hostname = "host",
                                osName = "Linux",
                                osVersion = "1",
                                kernelVersion = "6",
                                architecture = "amd64",
                                availableProcessors = 2,
                                totalMemoryBytes = 1024,
                                uptimeSeconds = 1,
                                agentVersion = "0.1.0"
                            )
                        }
                    )
                )
            )
        )
    }

    private fun command(
        type: String,
        parameters: Map<String, String> = emptyMap()
    ): CommandResponse {
        return CommandResponse(
            id = UUID.randomUUID(),
            deviceId = FIXED_DEVICE_ID,
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

    private class RecordingHubClient(
        private val nextCommand: CommandResponse? = null,
        private val fetchFailure: RuntimeException? = null
    ) : HubClient {

        var registration: RegisterDeviceRequest? = null
        var completedSuccess: Boolean? = null
        var completedResult: String? = null
        var completedError: String? = null
        var completedLeaseToken: UUID? = null
        var fetchCount: Int = 0
        var heartbeatCount: Int = 0
        var completeCount: Int = 0

        override fun register(request: RegisterDeviceRequest): DeviceResponse {
            registration = request
            return DeviceResponse(
                id = FIXED_DEVICE_ID,
                agentId = request.agentId,
                name = request.name,
                platform = request.platform,
                agentVersion = request.agentVersion,
                capabilities = request.capabilities,
                status = DeviceStatus.ONLINE,
                registeredAt = Instant.now(),
                lastSeenAt = Instant.now()
            )
        }

        override fun heartbeat(deviceId: UUID): DeviceResponse {
            heartbeatCount += 1
            return DeviceResponse(
                id = deviceId,
                agentId = FIXED_AGENT_ID,
                name = "test-agent",
                platform = "linux-amd64",
                agentVersion = "0.1.0",
                capabilities = emptySet(),
                status = DeviceStatus.ONLINE,
                registeredAt = Instant.now(),
                lastSeenAt = Instant.now()
            )
        }

        override fun fetchNextCommand(deviceId: UUID): CommandResponse? {
            fetchCount += 1
            fetchFailure?.let { throw it }
            return nextCommand
        }

        override fun completeCommand(
            deviceId: UUID,
            command: CommandResponse,
            success: Boolean,
            result: String?,
            error: String?
        ) {
            completeCount += 1
            completedSuccess = success
            completedResult = result
            completedError = error
            completedLeaseToken = command.leaseToken
        }
    }

    private class MutableTestClock : Clock() {
        private var currentInstant: Instant = Instant.parse("2026-01-01T00:00:00Z")

        override fun getZone(): ZoneId = ZoneId.of("UTC")

        override fun withZone(zone: ZoneId): Clock = this

        override fun instant(): Instant = currentInstant

        fun advance(duration: Duration) {
            currentInstant = currentInstant.plus(duration)
        }
    }

    companion object {
        private val FIXED_AGENT_ID = UUID.fromString("11111111-1111-1111-1111-111111111111")
        private val FIXED_DEVICE_ID = UUID.fromString("22222222-2222-2222-2222-222222222222")
    }
}
