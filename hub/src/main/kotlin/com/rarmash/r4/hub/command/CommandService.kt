package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import com.rarmash.r4.hub.device.DeviceRepository
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CommandStatus
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.command.CreateCommandRequest
import org.springframework.beans.factory.annotation.Value
import org.springframework.http.HttpStatus
import org.springframework.stereotype.Service
import org.springframework.web.server.ResponseStatusException
import java.time.Clock
import java.time.Instant
import java.util.UUID

@Service
class CommandService(
    private val commandRepository: CommandRepository,
    private val deviceRepository: DeviceRepository,

    @Value("\${r4.commands.lease-duration-ms:30000}")
    private val leaseDurationMs: Long,

    private val clock: Clock = Clock.systemUTC()
) {

    init {
        require(leaseDurationMs > 0) {
            "r4.commands.lease-duration-ms must be greater than zero"
        }
    }

    fun create(
        deviceId: UUID,
        request: CreateCommandRequest
    ): CommandResponse {
        val device = deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        val type = request.type.trim()

        if (type !in device.capabilities) {
            throw ResponseStatusException(
                HttpStatus.BAD_REQUEST,
                "Device does not support capability $type"
            )
        }

        val command = Command(
            id = UUID.randomUUID(),
            deviceId = deviceId,
            type = type,
            parameters = request.parameters,
            status = CommandStatus.PENDING,
            result = null,
            error = null,
            attemptCount = 0,
            leaseExpiresAt = null,
            leaseToken = null,
            createdAt = Instant.now(clock),
            startedAt = null,
            completedAt = null
        )

        return commandRepository
            .save(command)
            .toResponse()
    }

    fun claimNext(deviceId: UUID): CommandResponse? {
        deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        val now = Instant.now(clock)

        val command = commandRepository.claimNext(
            deviceId = deviceId,
            now = now,
            leaseExpiresAt = now.plusMillis(leaseDurationMs),
            leaseToken = UUID.randomUUID()
        )

        return command?.toResponse()
    }

    fun complete(
        deviceId: UUID,
        commandId: UUID,
        request: CompleteCommandRequest
    ): CommandResponse {
        val existingCommand =
            commandRepository.findById(commandId)
                ?: throw ResponseStatusException(
                    HttpStatus.NOT_FOUND,
                    "Command $commandId not found"
                )

        if (existingCommand.deviceId != deviceId) {
            throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Command $commandId does not belong to device $deviceId"
            )
        }

        if (existingCommand.status != CommandStatus.RUNNING) {
            throw ResponseStatusException(
                HttpStatus.CONFLICT,
                "Command $commandId is not running"
            )
        }

        val terminalStatus = if (request.success) {
            CommandStatus.SUCCEEDED
        } else {
            CommandStatus.FAILED
        }

        val completed = commandRepository.complete(
            commandId = commandId,
            deviceId = deviceId,
            leaseToken = request.leaseToken,
            completedAt = Instant.now(clock),
            status = terminalStatus,
            result = if (request.success) {
                request.result
            } else {
                null
            },
            error = if (request.success) {
                null
            } else {
                request.error ?: "Command failed"
            }
        ) ?: throw ResponseStatusException(
            HttpStatus.CONFLICT,
            "Command lease expired or is no longer owned by this agent"
        )

        return completed.toResponse()
    }

    fun getAll(deviceId: UUID): List<CommandResponse> {
        deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        return commandRepository
            .findAllByDeviceId(deviceId)
            .map { it.toResponse() }
    }

    private fun Command.toResponse(): CommandResponse {
        return CommandResponse(
            id = id,
            deviceId = deviceId,
            type = type,
            parameters = parameters,
            status = status,
            result = result,
            error = error,
            attemptCount = attemptCount,
            leaseExpiresAt = leaseExpiresAt,
            leaseToken = leaseToken,
            createdAt = createdAt,
            startedAt = startedAt,
            completedAt = completedAt
        )
    }
}
