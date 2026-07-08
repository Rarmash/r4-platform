package com.rarmash.r4.hub

import com.rarmash.r4.hub.command.CommandService
import com.rarmash.r4.hub.device.DeviceService
import com.rarmash.r4.protocol.command.CommandStatus
import com.rarmash.r4.protocol.command.CompleteCommandRequest
import com.rarmash.r4.protocol.command.CreateCommandRequest
import com.rarmash.r4.protocol.device.RegisterDeviceRequest
import org.assertj.core.api.Assertions.assertThat
import org.junit.jupiter.api.Test
import org.springframework.beans.factory.annotation.Autowired
import org.springframework.boot.test.context.SpringBootTest
import org.springframework.test.context.DynamicPropertyRegistry
import org.springframework.test.context.DynamicPropertySource
import org.testcontainers.junit.jupiter.Container
import org.testcontainers.junit.jupiter.Testcontainers
import org.testcontainers.postgresql.PostgreSQLContainer
import java.util.UUID

@Testcontainers
@SpringBootTest
class R4HubApplicationTest {

    @Autowired
    private lateinit var deviceService: DeviceService

    @Autowired
    private lateinit var commandService: CommandService

    @Test
    fun `persists device and command lifecycle in postgres`() {
        val device = deviceService.register(
            RegisterDeviceRequest(
                agentId = UUID.randomUUID(),
                name = "Integration simulator",
                platform = "linux-arm64",
                agentVersion = "0.1.0",
                capabilities = setOf("command.echo")
            )
        )

        val createdCommand = commandService.create(
            deviceId = device.id,
            request = CreateCommandRequest(
                type = "command.echo",
                parameters = mapOf(
                    "message" to "PostgreSQL works"
                )
            )
        )

        val claimedCommand = requireNotNull(
            commandService.claimNext(device.id)
        )

        val completedCommand = commandService.complete(
            deviceId = device.id,
            commandId = claimedCommand.id,
            request = CompleteCommandRequest(
                success = true,
                result = "PostgreSQL works"
            )
        )

        assertThat(createdCommand.status)
            .isEqualTo(CommandStatus.PENDING)

        assertThat(claimedCommand.status)
            .isEqualTo(CommandStatus.RUNNING)

        assertThat(completedCommand.status)
            .isEqualTo(CommandStatus.SUCCEEDED)

        assertThat(
            deviceService.getAll()
                .map { it.id }
        ).contains(device.id)

        assertThat(
            commandService.getAll(device.id)
                .single()
                .result
        ).isEqualTo("PostgreSQL works")
    }

    companion object {

        @Container
        @JvmField
        val postgres =
            PostgreSQLContainer("postgres:17.6-alpine")
                .withDatabaseName("r4")
                .withUsername("r4")
                .withPassword("r4-test")

        @DynamicPropertySource
        @JvmStatic
        fun configureDatabase(
            registry: DynamicPropertyRegistry
        ) {
            registry.add(
                "spring.datasource.url",
                postgres::getJdbcUrl
            )
            registry.add(
                "spring.datasource.username",
                postgres::getUsername
            )
            registry.add(
                "spring.datasource.password",
                postgres::getPassword
            )
        }
    }
}
