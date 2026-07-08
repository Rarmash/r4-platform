package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import com.rarmash.r4.protocol.command.CommandStatus
import java.time.Instant
import java.util.UUID

interface CommandRepository {

    fun save(command: Command): Command

    fun findById(commandId: UUID): Command?

    fun findAllByDeviceId(deviceId: UUID): List<Command>

    fun claimNext(
        deviceId: UUID,
        now: Instant,
        leaseExpiresAt: Instant,
        leaseToken: UUID
    ): Command?

    fun complete(
        commandId: UUID,
        deviceId: UUID,
        leaseToken: UUID,
        completedAt: Instant,
        status: CommandStatus,
        result: String?,
        error: String?
    ): Command?
}
