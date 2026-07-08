package com.rarmash.r4.protocol.command

import java.time.Instant
import java.util.UUID

data class CommandResponse(
    val id: UUID,
    val deviceId: UUID,
    val type: String,
    val parameters: Map<String, String>,
    val status: CommandStatus,
    val result: String?,
    val error: String?,
    val attemptCount: Int,
    val leaseExpiresAt: Instant?,
    val leaseToken: UUID?,
    val createdAt: Instant,
    val startedAt: Instant?,
    val completedAt: Instant?
)
