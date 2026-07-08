package com.rarmash.r4.agent.linux.command

import com.rarmash.r4.protocol.command.CommandResponse

class CommandDispatcher(
    handlers: Collection<CommandHandler>
) {

    private val handlersByType = handlers.associateBy { it.type }

    val capabilities: Set<String> = handlers
        .map { it.capability }
        .toSortedSet()

    fun dispatch(command: CommandResponse): CommandExecutionResult {
        val handler = handlersByType[command.type]
            ?: return CommandExecutionResult.failed("Unsupported command type: ${command.type}")

        return runCatching { handler.handle(command) }
            .getOrElse { exception ->
                CommandExecutionResult.failed(
                    exception.message ?: "Command failed with ${exception.javaClass.simpleName}"
                )
            }
    }
}
