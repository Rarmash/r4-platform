package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
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
            .sortedBy { it.createdAt }
    }
}
