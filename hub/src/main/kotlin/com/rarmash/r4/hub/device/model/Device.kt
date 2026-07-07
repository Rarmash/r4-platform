package com.rarmash.r4.hub.device.model

import com.rarmash.r4.protocol.device.DeviceStatus
import java.time.Instant
import java.util.UUID

data class Device(
    val id: UUID,
    val agentId: UUID,
    val name: String,
    val platform: String,
    val agentVersion: String,
    val capabilities: Set<String>,
    val status: DeviceStatus,
    val registeredAt: Instant,
    val lastSeenAt: Instant
)