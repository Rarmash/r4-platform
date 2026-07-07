package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.device.model.Device
import java.util.UUID

interface DeviceRepository {

    fun save(device: Device): Device

    fun findById(deviceId: UUID): Device?

    fun findByAgentId(agentId: UUID): Device?

    fun findAll(): List<Device>
}