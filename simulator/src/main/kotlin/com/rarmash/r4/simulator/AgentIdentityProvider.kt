package com.rarmash.r4.simulator

import org.springframework.beans.factory.annotation.Value
import org.springframework.stereotype.Component
import java.nio.file.Files
import java.nio.file.Path
import java.util.UUID

@Component
class AgentIdentityProvider(
    @Value("\${r4.identity-file}") identityFile: String
) {

    private val path = Path.of(identityFile)

    val agentId: UUID by lazy {
        loadOrCreate()
    }

    private fun loadOrCreate(): UUID {
        if (Files.exists(path)) {
            val value = Files.readString(path).trim()
            return UUID.fromString(value)
        }

        val newAgentId = UUID.randomUUID()

        path.parent?.let {
            Files.createDirectories(it)
        }

        Files.writeString(path, newAgentId.toString())

        return newAgentId
    }
}