package com.rarmash.r4.agent.linux.command

import com.fasterxml.jackson.databind.ObjectMapper
import com.rarmash.r4.agent.linux.hub.HttpHubClient
import com.rarmash.r4.agent.linux.system.SystemInfoCollector
import com.rarmash.r4.protocol.command.CommandResponse

class SystemInfoCommandHandler(
    private val systemInfoCollector: SystemInfoCollector,
    private val objectMapper: ObjectMapper = HttpHubClient.defaultObjectMapper()
) : CommandHandler {

    override val type: String = "system.info"

    override fun handle(command: CommandResponse): CommandExecutionResult {
        return CommandExecutionResult.succeeded(
            objectMapper.writeValueAsString(systemInfoCollector.collect())
        )
    }
}
