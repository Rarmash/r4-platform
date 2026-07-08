package com.rarmash.r4.agent.linux.command

import com.rarmash.r4.protocol.command.CommandResponse

class EchoCommandHandler : CommandHandler {
    override val type: String = "command.echo"

    override fun handle(command: CommandResponse): CommandExecutionResult {
        return CommandExecutionResult.succeeded(command.parameters["message"] ?: "")
    }
}
