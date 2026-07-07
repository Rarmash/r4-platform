package com.rarmash.r4.simulator

import org.assertj.core.api.Assertions.assertThat
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.io.TempDir
import java.nio.file.Files
import java.nio.file.Path

class AgentIdentityProviderTest {

    @TempDir
    lateinit var tempDirectory: Path

    @Test
    fun `identity is persisted and reused`() {
        val identityFile = tempDirectory.resolve("agent-id")

        val firstProvider = AgentIdentityProvider(
            identityFile.toString()
        )

        val firstAgentId = firstProvider.agentId

        val secondProvider = AgentIdentityProvider(
            identityFile.toString()
        )

        val secondAgentId = secondProvider.agentId

        assertThat(secondAgentId).isEqualTo(firstAgentId)

        assertThat(Files.readString(identityFile).trim())
            .isEqualTo(firstAgentId.toString())
    }
}