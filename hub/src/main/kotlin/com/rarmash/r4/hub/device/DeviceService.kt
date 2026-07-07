package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.device.model.Device
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.DeviceStatus
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.springframework.beans.factory.annotation.Value
import org.springframework.http.HttpStatus
import org.springframework.stereotype.Service
import org.springframework.web.server.ResponseStatusException
import java.time.Clock
import java.time.Instant
import java.util.UUID

@Service
class DeviceService(
    private val deviceRepository: DeviceRepository,

    @Value("\${r4.devices.offline-after-ms:25000}")
    private val offlineAfterMs: Long,

    private val clock: Clock = Clock.systemUTC()
) {

    init {
        require(offlineAfterMs > 0) {
            "r4.devices.offline-after-ms must be greater than zero"
        }
    }

    fun register(request: RegisterDeviceRequest): DeviceResponse {
        val now = Instant.now(clock)
        val existingDevice =
            deviceRepository.findByAgentId(request.agentId)

        val device = if (existingDevice == null) {
            Device(
                id = UUID.randomUUID(),
                agentId = request.agentId,
                name = request.name.trim(),
                platform = request.platform.trim(),
                agentVersion = request.agentVersion.trim(),
                capabilities = request.capabilities,
                registeredAt = now,
                lastSeenAt = now
            )
        } else {
            existingDevice.copy(
                name = request.name.trim(),
                platform = request.platform.trim(),
                agentVersion = request.agentVersion.trim(),
                capabilities = request.capabilities,
                lastSeenAt = now
            )
        }

        return deviceRepository
            .save(device)
            .toResponse(now)
    }

    fun heartbeat(deviceId: UUID): DeviceResponse {
        val device = deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        val now = Instant.now(clock)

        val updatedDevice = device.copy(
            lastSeenAt = now
        )

        return deviceRepository
            .save(updatedDevice)
            .toResponse(now)
    }

    fun getAll(): List<DeviceResponse> {
        val now = Instant.now(clock)

        return deviceRepository.findAll()
            .map { device -> device.toResponse(now) }
    }

    private fun Device.toResponse(now: Instant): DeviceResponse {
        return DeviceResponse(
            id = id,
            agentId = agentId,
            name = name,
            platform = platform,
            agentVersion = agentVersion,
            capabilities = capabilities,
            status = determineStatus(now),
            registeredAt = registeredAt,
            lastSeenAt = lastSeenAt
        )
    }

    private fun Device.determineStatus(now: Instant): DeviceStatus {
        val offlineAt = lastSeenAt.plusMillis(offlineAfterMs)

        return if (now.isBefore(offlineAt)) {
            DeviceStatus.ONLINE
        } else {
            DeviceStatus.OFFLINE
        }
    }
}