package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.device.model.Device
import org.springframework.stereotype.Repository
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

@Repository
class InMemoryDeviceRepository : DeviceRepository {

    private val devices = ConcurrentHashMap<UUID, Device>()

    override fun save(device: Device): Device {
        devices[device.id] = device
        return device
    }

    override fun findById(deviceId: UUID): Device? {
        return devices[deviceId]
    }

    override fun findAll(): List<Device> {
        return devices.values
            .sortedBy { it.registeredAt }
    }
}