package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.device.model.Device
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.DeviceStatus
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.springframework.http.HttpStatus
import org.springframework.stereotype.Service
import org.springframework.web.server.ResponseStatusException
import java.time.Clock
import java.time.Instant
import java.util.UUID

@Service
class DeviceService(
    private val deviceRepository: DeviceRepository,
    private val clock: Clock = Clock.systemUTC()
) {

    fun register(request: RegisterDeviceRequest): DeviceResponse {
        val now = Instant.now(clock)
        val existingDevice = deviceRepository.findByAgentId(request.agentId)

        val device = if (existingDevice == null) {
            Device(
                id = UUID.randomUUID(),
                agentId = request.agentId,
                name = request.name.trim(),
                platform = request.platform.trim(),
                agentVersion = request.agentVersion.trim(),
                capabilities = request.capabilities,
                status = DeviceStatus.ONLINE,
                registeredAt = now,
                lastSeenAt = now
            )
        } else {
            existingDevice.copy(
                name = request.name.trim(),
                platform = request.platform.trim(),
                agentVersion = request.agentVersion.trim(),
                capabilities = request.capabilities,
                status = DeviceStatus.ONLINE,
                lastSeenAt = now
            )
        }

        return deviceRepository.save(device).toResponse()
    }

    fun heartbeat(deviceId: UUID): DeviceResponse {
        val device = deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        val updatedDevice = device.copy(
            status = DeviceStatus.ONLINE,
            lastSeenAt = Instant.now(clock)
        )

        return deviceRepository.save(updatedDevice).toResponse()
    }

    fun getAll(): List<DeviceResponse> {
        return deviceRepository.findAll()
            .map { it.toResponse() }
    }

    private fun Device.toResponse(): DeviceResponse {
        return DeviceResponse(
            id = id,
            agentId = agentId,
            name = name,
            platform = platform,
            agentVersion = agentVersion,
            capabilities = capabilities,
            status = status,
            registeredAt = registeredAt,
            lastSeenAt = lastSeenAt
        )
    }
}