package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import java.util.UUID

interface CommandRepository {

    fun save(command: Command): Command

    fun findById(commandId: UUID): Command?

    fun findAllByDeviceId(deviceId: UUID): List<Command>
}