package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.support.MutableClock
import com.rarmash.r4.protocol.device.DeviceStatus
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

class DeviceServiceTest {

    private lateinit var repository: InMemoryDeviceRepository
    private lateinit var clock: MutableClock
    private lateinit var service: DeviceService

    @BeforeEach
    fun setUp() {
        repository = InMemoryDeviceRepository()

        clock = MutableClock(
            Instant.parse("2026-07-07T20:00:00Z")
        )

        service = DeviceService(
            deviceRepository = repository,
            offlineAfterMs = 25_000,
            clock = clock
        )
    }

    @Test
    fun `registers new device as online`() {
        val agentId = UUID.randomUUID()

        val device = service.register(
            request(agentId = agentId)
        )

        assertThat(device.id).isNotNull()
        assertThat(device.agentId).isEqualTo(agentId)
        assertThat(device.name).isEqualTo("Test simulator")
        assertThat(device.status).isEqualTo(DeviceStatus.ONLINE)
        assertThat(device.registeredAt).isEqualTo(clock.instant())
        assertThat(device.lastSeenAt).isEqualTo(clock.instant())

        assertThat(repository.findById(device.id)).isNotNull()
    }

    @Test
    fun `re-registering same agent preserves device identity`() {
        val agentId = UUID.randomUUID()

        val firstRegistration = service.register(
            request(agentId = agentId)
        )

        clock.advance(Duration.ofSeconds(10))

        val secondRegistration = service.register(
            request(
                agentId = agentId,
                name = "Renamed simulator",
                agentVersion = "0.2.0"
            )
        )

        assertThat(secondRegistration.id)
            .isEqualTo(firstRegistration.id)

        assertThat(secondRegistration.agentId)
            .isEqualTo(firstRegistration.agentId)

        assertThat(secondRegistration.registeredAt)
            .isEqualTo(firstRegistration.registeredAt)

        assertThat(secondRegistration.lastSeenAt)
            .isEqualTo(clock.instant())

        assertThat(secondRegistration.name)
            .isEqualTo("Renamed simulator")

        assertThat(secondRegistration.agentVersion)
            .isEqualTo("0.2.0")

        assertThat(service.getAll()).hasSize(1)
    }

    @Test
    fun `heartbeat for unknown device returns not found`() {
        val exception = assertThrows(
            ResponseStatusException::class.java
        ) {
            service.heartbeat(UUID.randomUUID())
        }

        assertThat(exception.statusCode)
            .isEqualTo(HttpStatus.NOT_FOUND)
    }

    @Test
    fun `device becomes offline and heartbeat restores online status`() {
        val device = service.register(
            request(agentId = UUID.randomUUID())
        )

        clock.advance(Duration.ofMillis(24_999))

        assertThat(service.getAll().single().status)
            .isEqualTo(DeviceStatus.ONLINE)

        clock.advance(Duration.ofMillis(1))

        assertThat(service.getAll().single().status)
            .isEqualTo(DeviceStatus.OFFLINE)

        val updatedDevice = service.heartbeat(device.id)

        assertThat(updatedDevice.status)
            .isEqualTo(DeviceStatus.ONLINE)

        assertThat(updatedDevice.lastSeenAt)
            .isEqualTo(clock.instant())
    }

    private fun request(
        agentId: UUID,
        name: String = "Test simulator",
        agentVersion: String = "0.1.0"
    ): RegisterDeviceRequest {
        return RegisterDeviceRequest(
            agentId = agentId,
            name = name,
            platform = "windows-amd64",
            agentVersion = agentVersion,
            capabilities = setOf(
                "system-info",
                "command.echo"
            )
        )
    }
}