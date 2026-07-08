package com.rarmash.r4.agent.linux.identity

import java.nio.file.Files
import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.util.UUID

class PersistentAgentIdentity(
    dataDir: Path
) : AgentIdentity {

    private val identityFile = dataDir.resolve("agent-id")

    override val agentId: UUID by lazy {
        loadOrCreate()
    }

    private fun loadOrCreate(): UUID {
        if (Files.exists(identityFile)) {
            val value = Files.readString(identityFile).trim()
            return runCatching { UUID.fromString(value) }
                .getOrElse {
                    createNew()
                }
        }

        return createNew()
    }

    private fun createNew(): UUID {
        val newAgentId = UUID.randomUUID()
        identityFile.parent?.let(Files::createDirectories)

        val temporaryFile = Files.createTempFile(
            identityFile.parent,
            "agent-id-",
            ".tmp"
        )

        Files.writeString(temporaryFile, newAgentId.toString())
        runCatching {
            Files.move(
                temporaryFile,
                identityFile,
                StandardCopyOption.REPLACE_EXISTING,
                StandardCopyOption.ATOMIC_MOVE
            )
        }.getOrElse { exception ->
            if (exception is AtomicMoveNotSupportedException) {
                Files.move(
                    temporaryFile,
                    identityFile,
                    StandardCopyOption.REPLACE_EXISTING
                )
            } else {
                throw exception
            }
        }

        return newAgentId
    }
}
