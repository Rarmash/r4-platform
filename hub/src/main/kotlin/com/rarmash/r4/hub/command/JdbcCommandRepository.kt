package com.rarmash.r4.hub.command

import com.rarmash.r4.hub.command.model.Command
import com.rarmash.r4.hub.persistence.JsonCodec
import com.rarmash.r4.protocol.command.CommandStatus
import org.springframework.jdbc.core.RowMapper
import org.springframework.jdbc.core.simple.JdbcClient
import org.springframework.stereotype.Repository
import java.sql.ResultSet
import java.time.OffsetDateTime
import java.time.ZoneOffset
import java.util.UUID

@Repository
class JdbcCommandRepository(
    private val jdbcClient: JdbcClient,
    private val jsonCodec: JsonCodec
) : CommandRepository {

    private val rowMapper = RowMapper<Command> { resultSet, _ ->
        mapCommand(resultSet)
    }

    override fun save(command: Command): Command {
        jdbcClient.sql(
            """
            INSERT INTO device_commands (
                id,
                device_id,
                type,
                parameters,
                status,
                result,
                error,
                created_at,
                started_at,
                completed_at
            )
            VALUES (
                :id,
                :deviceId,
                :type,
                CAST(:parameters AS jsonb),
                :status,
                :result,
                :error,
                :createdAt,
                :startedAt,
                :completedAt
            )
            ON CONFLICT (id) DO UPDATE SET
                device_id = EXCLUDED.device_id,
                type = EXCLUDED.type,
                parameters = EXCLUDED.parameters,
                status = EXCLUDED.status,
                result = EXCLUDED.result,
                error = EXCLUDED.error,
                started_at = EXCLUDED.started_at,
                completed_at = EXCLUDED.completed_at
            """.trimIndent()
        )
            .param("id", command.id)
            .param("deviceId", command.deviceId)
            .param("type", command.type)
            .param(
                "parameters",
                jsonCodec.write(command.parameters)
            )
            .param("status", command.status.name)
            .param("result", command.result)
            .param("error", command.error)
            .param(
                "createdAt",
                command.createdAt.atOffset(ZoneOffset.UTC)
            )
            .param(
                "startedAt",
                command.startedAt?.atOffset(ZoneOffset.UTC)
            )
            .param(
                "completedAt",
                command.completedAt?.atOffset(ZoneOffset.UTC)
            )
            .update()

        return command
    }

    override fun findById(commandId: UUID): Command? {
        return jdbcClient.sql(
            """
            SELECT *
            FROM device_commands
            WHERE id = :commandId
            """.trimIndent()
        )
            .param("commandId", commandId)
            .query(rowMapper)
            .optional()
            .orElse(null)
    }

    override fun findAllByDeviceId(
        deviceId: UUID
    ): List<Command> {
        return jdbcClient.sql(
            """
            SELECT *
            FROM device_commands
            WHERE device_id = :deviceId
            ORDER BY created_at, id
            """.trimIndent()
        )
            .param("deviceId", deviceId)
            .query(rowMapper)
            .list()
    }

    private fun mapCommand(resultSet: ResultSet): Command {
        return Command(
            id = resultSet.getObject(
                "id",
                UUID::class.java
            ),
            deviceId = resultSet.getObject(
                "device_id",
                UUID::class.java
            ),
            type = resultSet.getString("type"),
            parameters = jsonCodec.readStringMap(
                resultSet.getString("parameters")
            ),
            status = CommandStatus.valueOf(
                resultSet.getString("status")
            ),
            result = resultSet.getString("result"),
            error = resultSet.getString("error"),
            createdAt = resultSet.getObject(
                "created_at",
                OffsetDateTime::class.java
            ).toInstant(),
            startedAt = resultSet.getObject(
                "started_at",
                OffsetDateTime::class.java
            )?.toInstant(),
            completedAt = resultSet.getObject(
                "completed_at",
                OffsetDateTime::class.java
            )?.toInstant()
        )
    }
}
