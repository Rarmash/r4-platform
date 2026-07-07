package com.rarmash.r4.protocol.device

import java.time.Instant
import java.util.UUID

data class DeviceResponse(
    val id: UUID,
    val name: String,
    val platform: String,
    val agentVersion: String,
    val capabilities: Set<String>,
    val status: DeviceStatus,
    val registeredAt: Instant,
    val lastSeenAt: Instant
)