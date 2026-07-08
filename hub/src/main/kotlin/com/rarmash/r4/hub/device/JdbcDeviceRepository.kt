package com.rarmash.r4.hub.device

import com.rarmash.r4.hub.device.model.Device
import com.rarmash.r4.hub.persistence.JsonCodec
import org.springframework.jdbc.core.RowMapper
import org.springframework.jdbc.core.simple.JdbcClient
import org.springframework.stereotype.Repository
import java.sql.ResultSet
import java.time.OffsetDateTime
import java.time.ZoneOffset
import java.util.UUID

@Repository
class JdbcDeviceRepository(
    private val jdbcClient: JdbcClient,
    private val jsonCodec: JsonCodec
) : DeviceRepository {

    private val rowMapper = RowMapper<Device> { resultSet, _ ->
        mapDevice(resultSet)
    }

    override fun save(device: Device): Device {
        jdbcClient.sql(
            """
            INSERT INTO devices (
                id,
                agent_id,
                name,
                platform,
                agent_version,
                capabilities,
                registered_at,
                last_seen_at
            )
            VALUES (
                :id,
                :agentId,
                :name,
                :platform,
                :agentVersion,
                CAST(:capabilities AS jsonb),
                :registeredAt,
                :lastSeenAt
            )
            ON CONFLICT (id) DO UPDATE SET
                agent_id = EXCLUDED.agent_id,
                name = EXCLUDED.name,
                platform = EXCLUDED.platform,
                agent_version = EXCLUDED.agent_version,
                capabilities = EXCLUDED.capabilities,
                last_seen_at = EXCLUDED.last_seen_at
            """.trimIndent()
        )
            .param("id", device.id)
            .param("agentId", device.agentId)
            .param("name", device.name)
            .param("platform", device.platform)
            .param("agentVersion", device.agentVersion)
            .param(
                "capabilities",
                jsonCodec.write(device.capabilities)
            )
            .param(
                "registeredAt",
                device.registeredAt.atOffset(ZoneOffset.UTC)
            )
            .param(
                "lastSeenAt",
                device.lastSeenAt.atOffset(ZoneOffset.UTC)
            )
            .update()

        return device
    }

    override fun findById(deviceId: UUID): Device? {
        return jdbcClient.sql(
            """
            SELECT *
            FROM devices
            WHERE id = :deviceId
            """.trimIndent()
        )
            .param("deviceId", deviceId)
            .query(rowMapper)
            .optional()
            .orElse(null)
    }

    override fun findByAgentId(agentId: UUID): Device? {
        return jdbcClient.sql(
            """
            SELECT *
            FROM devices
            WHERE agent_id = :agentId
            """.trimIndent()
        )
            .param("agentId", agentId)
            .query(rowMapper)
            .optional()
            .orElse(null)
    }

    override fun findAll(): List<Device> {
        return jdbcClient.sql(
            """
            SELECT *
            FROM devices
            ORDER BY registered_at, id
            """.trimIndent()
        )
            .query(rowMapper)
            .list()
    }

    private fun mapDevice(resultSet: ResultSet): Device {
        return Device(
            id = resultSet.getObject(
                "id",
                UUID::class.java
            ),
            agentId = resultSet.getObject(
                "agent_id",
                UUID::class.java
            ),
            name = resultSet.getString("name"),
            platform = resultSet.getString("platform"),
            agentVersion = resultSet.getString("agent_version"),
            capabilities = jsonCodec.readStringSet(
                resultSet.getString("capabilities")
            ),
            registeredAt = resultSet.getObject(
                "registered_at",
                OffsetDateTime::class.java
            ).toInstant(),
            lastSeenAt = resultSet.getObject(
                "last_seen_at",
                OffsetDateTime::class.java
            ).toInstant()
        )
    }
}
