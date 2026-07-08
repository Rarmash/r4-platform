package com.rarmash.r4.agent.linux.command

import com.rarmash.r4.protocol.command.CommandResponse

interface CommandHandler {
    val type: String

    val capability: String
        get() = type

    fun handle(command: CommandResponse): CommandExecutionResult
}
