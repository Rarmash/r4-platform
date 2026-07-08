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
                attempt_count,
                lease_expires_at,
                lease_token,
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
                :attemptCount,
                :leaseExpiresAt,
                :leaseToken,
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
                attempt_count = EXCLUDED.attempt_count,
                lease_expires_at = EXCLUDED.lease_expires_at,
                lease_token = EXCLUDED.lease_token,
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
            .param("attemptCount", command.attemptCount)
            .param(
                "leaseExpiresAt",
                command.leaseExpiresAt
                    ?.atOffset(ZoneOffset.UTC)
            )
            .param("leaseToken", command.leaseToken)
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

    override fun claimNext(
        deviceId: UUID,
        now: java.time.Instant,
        leaseExpiresAt: java.time.Instant,
        leaseToken: UUID
    ): Command? {
        return jdbcClient.sql(
            """
            WITH candidate AS (
                SELECT id
                FROM device_commands
                WHERE device_id = :deviceId
                  AND (
                      status = 'PENDING'
                      OR (
                          status = 'RUNNING'
                          AND lease_expires_at IS NOT NULL
                          AND lease_expires_at <= :now
                      )
                  )
                ORDER BY created_at, id
                FOR UPDATE SKIP LOCKED
                LIMIT 1
            )
            UPDATE device_commands AS command
            SET
                status = 'RUNNING',
                result = NULL,
                error = NULL,
                attempt_count = command.attempt_count + 1,
                lease_expires_at = :leaseExpiresAt,
                lease_token = :leaseToken,
                started_at = :now,
                completed_at = NULL
            FROM candidate
            WHERE command.id = candidate.id
            RETURNING command.*
            """.trimIndent()
        )
            .param("deviceId", deviceId)
            .param("now", now.atOffset(ZoneOffset.UTC))
            .param(
                "leaseExpiresAt",
                leaseExpiresAt.atOffset(ZoneOffset.UTC)
            )
            .param("leaseToken", leaseToken)
            .query(rowMapper)
            .optional()
            .orElse(null)
    }

    override fun complete(
        commandId: UUID,
        deviceId: UUID,
        leaseToken: UUID,
        completedAt: java.time.Instant,
        status: CommandStatus,
        result: String?,
        error: String?
    ): Command? {
        return jdbcClient.sql(
            """
            UPDATE device_commands
            SET
                status = :status,
                result = :result,
                error = :error,
                completed_at = :completedAt,
                lease_expires_at = NULL,
                lease_token = NULL
            WHERE id = :commandId
              AND device_id = :deviceId
              AND status = 'RUNNING'
              AND lease_token = :leaseToken
              AND lease_expires_at > :completedAt
            RETURNING *
            """.trimIndent()
        )
            .param("status", status.name)
            .param("result", result)
            .param("error", error)
            .param(
                "completedAt",
                completedAt.atOffset(ZoneOffset.UTC)
            )
            .param("commandId", commandId)
            .param("deviceId", deviceId)
            .param("leaseToken", leaseToken)
            .query(rowMapper)
            .optional()
            .orElse(null)
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
            attemptCount = resultSet.getInt("attempt_count"),
            leaseExpiresAt = resultSet.getObject(
                "lease_expires_at",
                OffsetDateTime::class.java
            )?.toInstant(),
            leaseToken = resultSet.getObject(
                "lease_token",
                UUID::class.java
            ),
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
