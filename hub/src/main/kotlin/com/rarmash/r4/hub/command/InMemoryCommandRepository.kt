package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import com.rarmash.r4.protocol.command.CommandStatus
import java.time.Instant
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

class InMemoryCommandRepository : CommandRepository {

    private val commands = ConcurrentHashMap<UUID, Command>()

    override fun save(command: Command): Command {
        commands[command.id] = command
        return command
    }

    override fun findById(commandId: UUID): Command? {
        return commands[commandId]
    }

    override fun findAllByDeviceId(deviceId: UUID): List<Command> {
        return commands.values
            .filter { it.deviceId == deviceId }
            .sortedWith(
                compareBy<Command> { it.createdAt }
                    .thenBy { it.id }
            )
    }

    @Synchronized
    override fun claimNext(
        deviceId: UUID,
        now: Instant,
        leaseExpiresAt: Instant,
        leaseToken: UUID
    ): Command? {
        val command = commands.values
            .filter { it.deviceId == deviceId }
            .filter {
                it.status == CommandStatus.PENDING ||
                    (
                        it.status == CommandStatus.RUNNING &&
                            it.leaseExpiresAt != null &&
                            !it.leaseExpiresAt.isAfter(now)
                        )
            }
            .minWithOrNull(
                compareBy<Command> { it.createdAt }
                    .thenBy { it.id }
            )
            ?: return null

        val claimed = command.copy(
            status = CommandStatus.RUNNING,
            result = null,
            error = null,
            attemptCount = command.attemptCount + 1,
            leaseExpiresAt = leaseExpiresAt,
            leaseToken = leaseToken,
            startedAt = now,
            completedAt = null
        )

        commands[claimed.id] = claimed
        return claimed
    }

    @Synchronized
    override fun complete(
        commandId: UUID,
        deviceId: UUID,
        leaseToken: UUID,
        completedAt: Instant,
        status: CommandStatus,
        result: String?,
        error: String?
    ): Command? {
        val command = commands[commandId]
            ?: return null

        val leaseExpiresAt = command.leaseExpiresAt
            ?: return null

        if (
            command.deviceId != deviceId ||
            command.status != CommandStatus.RUNNING ||
            command.leaseToken != leaseToken ||
            !leaseExpiresAt.isAfter(completedAt)
        ) {
            return null
        }

        val completed = command.copy(
            status = status,
            result = result,
            error = error,
            completedAt = completedAt,
            leaseExpiresAt = null,
            leaseToken = null
        )

        commands[commandId] = completed
        return completed
    }
}
