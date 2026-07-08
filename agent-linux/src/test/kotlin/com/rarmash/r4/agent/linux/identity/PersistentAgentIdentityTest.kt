package com.rarmash.r4.agent.linux.identity

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals
import java.nio.file.Files
import java.util.UUID

class PersistentAgentIdentityTest {

    @Test
    fun `creates agent id when file does not exist`() {
        val dataDir = Files.createTempDirectory("r4-agent-test-")

        val agentId = PersistentAgentIdentity(dataDir).agentId

        assertEquals(agentId.toString(), Files.readString(dataDir.resolve("agent-id")))
    }

    @Test
    fun `reuses existing agent id`() {
        val dataDir = Files.createTempDirectory("r4-agent-test-")
        val existingAgentId = UUID.randomUUID()
        Files.writeString(dataDir.resolve("agent-id"), existingAgentId.toString())

        val agentId = PersistentAgentIdentity(dataDir).agentId

        assertEquals(existingAgentId, agentId)
    }

    @Test
    fun `replaces corrupted agent id file`() {
        val dataDir = Files.createTempDirectory("r4-agent-test-")
        Files.writeString(dataDir.resolve("agent-id"), "not-a-uuid")

        val agentId = PersistentAgentIdentity(dataDir).agentId

        assertNotEquals("not-a-uuid", Files.readString(dataDir.resolve("agent-id")))
        assertEquals(agentId.toString(), Files.readString(dataDir.resolve("agent-id")))
    }
}
