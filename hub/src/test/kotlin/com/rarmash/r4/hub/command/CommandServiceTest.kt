package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.device.DeviceService
import com.rarmash.r4.hub.device.InMemoryDeviceRepository
import com.rarmash.r4.hub.support.MutableClock
import com.rarmash.r4.protocol.command.CommandStatus
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.command.CreateCommandRequest
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.assertj.core.api.Assertions.assertThat
import org.junit.jupiter.api.Assertions.assertThrows
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import org.springframework.http.HttpStatus
import org.springframework.web.server.ResponseStatusException
import java.time.Duration
import java.time.Instant
import java.util.UUID

class CommandServiceTest {

    private lateinit var deviceRepository: InMemoryDeviceRepository
    private lateinit var commandRepository: InMemoryCommandRepository
    private lateinit var clock: MutableClock
    private lateinit var deviceService: DeviceService
    private lateinit var commandService: CommandService

    @BeforeEach
    fun setUp() {
        deviceRepository = InMemoryDeviceRepository()
        commandRepository = InMemoryCommandRepository()

        clock = MutableClock(
            Instant.parse("2026-07-07T20:00:00Z")
        )

        deviceService = DeviceService(
            deviceRepository = deviceRepository,
            offlineAfterMs = 25_000,
            clock = clock
        )

        commandService = CommandService(
            commandRepository = commandRepository,
            deviceRepository = deviceRepository,
            clock = clock
        )
    }

    @Test
    fun `creates supported command as pending`() {
        val device = registerDevice()

        val command = commandService.create(
            deviceId = device.id,
            request = CreateCommandRequest(
                type = "command.echo",
                parameters = mapOf(
                    "message" to "Hello from test"
                )
            )
        )

        assertThat(command.deviceId).isEqualTo(device.id)
        assertThat(command.type).isEqualTo("command.echo")
        assertThat(command.status).isEqualTo(CommandStatus.PENDING)
        assertThat(command.result).isNull()
        assertThat(command.error).isNull()
        assertThat(command.startedAt).isNull()
        assertThat(command.completedAt).isNull()
    }

    @Test
    fun `rejects unsupported command`() {
        val device = registerDevice()

        val exception = assertThrows(
            ResponseStatusException::class.java
        ) {
            commandService.create(
                deviceId = device.id,
                request = CreateCommandRequest(
                    type = "command.shutdown"
                )
            )
        }

        assertThat(exception.statusCode)
            .isEqualTo(HttpStatus.BAD_REQUEST)

        assertThat(exception.reason)
            .contains("does not support")
    }

    @Test
    fun `command passes through complete lifecycle`() {
        val device = registerDevice()

        val created = commandService.create(
            deviceId = device.id,
            request = CreateCommandRequest(
                type = "command.echo",
                parameters = mapOf(
                    "message" to "Hello"
                )
            )
        )

        assertThat(created.status)
            .isEqualTo(CommandStatus.PENDING)

        clock.advance(Duration.ofSeconds(1))

        val claimed = requireNotNull(
            commandService.claimNext(device.id)
        )

        assertThat(claimed.id).isEqualTo(created.id)
        assertThat(claimed.status).isEqualTo(CommandStatus.RUNNING)
        assertThat(claimed.startedAt).isEqualTo(clock.instant())

        clock.advance(Duration.ofSeconds(1))

        val completed = commandService.complete(
            deviceId = device.id,
            commandId = created.id,
            request = CompleteCommandRequest(
                success = true,
                result = "Hello"
            )
        )

        assertThat(completed.status)
            .isEqualTo(CommandStatus.SUCCEEDED)

        assertThat(completed.result).isEqualTo("Hello")
        assertThat(completed.error).isNull()
        assertThat(completed.completedAt).isEqualTo(clock.instant())

        val storedCommand = commandService
            .getAll(device.id)
            .single()

        assertThat(storedCommand)
            .isEqualTo(completed)
    }

    @Test
    fun `completed command cannot be completed again`() {
        val device = registerDevice()

        val created = commandService.create(
            deviceId = device.id,
            request = CreateCommandRequest(
                type = "command.echo"
            )
        )

        commandService.claimNext(device.id)

        commandService.complete(
            deviceId = device.id,
            commandId = created.id,
            request = CompleteCommandRequest(
                success = true,
                result = "Done"
            )
        )

        val exception = assertThrows(
            ResponseStatusException::class.java
        ) {
            commandService.complete(
                deviceId = device.id,
                commandId = created.id,
                request = CompleteCommandRequest(
                    success = true,
                    result = "Done again"
                )
            )
        }

        assertThat(exception.statusCode)
            .isEqualTo(HttpStatus.CONFLICT)
    }

    @Test
    fun `claim returns null when there are no pending commands`() {
        val device = registerDevice()

        val command = commandService.claimNext(device.id)

        assertThat(command).isNull()
    }

    private fun registerDevice(): DeviceResponse {
        return deviceService.register(
            RegisterDeviceRequest(
                agentId = UUID.randomUUID(),
                name = "Test simulator",
                platform = "windows-amd64",
                agentVersion = "0.1.0",
                capabilities = setOf(
                    "system-info",
                    "command.echo"
                )
            )
        )
    }
}