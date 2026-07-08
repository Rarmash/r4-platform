package com.rarmash.r4.agent.linux.hub

import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.device.DeviceResponse
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import java.util.UUID

interface HubClient {
    fun register(request: RegisterDeviceRequest): DeviceResponse

    fun heartbeat(deviceId: UUID): DeviceResponse

    fun fetchNextCommand(deviceId: UUID): CommandResponse?

    fun completeCommand(
        deviceId: UUID,
        command: CommandResponse,
        success: Boolean,
        result: String? = null,
        error: String? = null
    )
}
