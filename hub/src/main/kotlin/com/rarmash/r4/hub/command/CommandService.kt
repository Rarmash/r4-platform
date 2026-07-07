package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import com.rarmash.r4.hub.device.DeviceRepository
import com.rarmash.r4.protocol.command.CommandResponse
import com.rarmash.r4.protocol.command.CommandStatus
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.command.CreateCommandRequest
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
    private val clock: Clock = Clock.systemUTC()
) {

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
            createdAt = Instant.now(clock),
            startedAt = null,
            completedAt = null
        )

        return commandRepository.save(command).toResponse()
    }

    @Synchronized
    fun claimNext(deviceId: UUID): CommandResponse? {
        deviceRepository.findById(deviceId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Device $deviceId not found"
            )

        val command = commandRepository
            .findAllByDeviceId(deviceId)
            .firstOrNull { it.status == CommandStatus.PENDING }
            ?: return null

        val runningCommand = command.copy(
            status = CommandStatus.RUNNING,
            startedAt = Instant.now(clock)
        )

        return commandRepository
            .save(runningCommand)
            .toResponse()
    }

    fun complete(
        deviceId: UUID,
        commandId: UUID,
        request: CompleteCommandRequest
    ): CommandResponse {
        val command = commandRepository.findById(commandId)
            ?: throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Command $commandId not found"
            )

        if (command.deviceId != deviceId) {
            throw ResponseStatusException(
                HttpStatus.NOT_FOUND,
                "Command $commandId does not belong to device $deviceId"
            )
        }

        if (command.status != CommandStatus.RUNNING) {
            throw ResponseStatusException(
                HttpStatus.CONFLICT,
                "Command $commandId is not running"
            )
        }

        val completedCommand = command.copy(
            status = if (request.success) {
                CommandStatus.SUCCEEDED
            } else {
                CommandStatus.FAILED
            },
            result = if (request.success) request.result else null,
            error = if (request.success) {
                null
            } else {
                request.error ?: "Command failed"
            },
            completedAt = Instant.now(clock)
        )

        return commandRepository
            .save(completedCommand)
            .toResponse()
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
            createdAt = createdAt,
            startedAt = startedAt,
            completedAt = completedAt
        )
    }
}