--liquibase formatted sql

--changeset rarmash:001-initial-schema

CREATE TABLE devices
(
    id            UUID                     PRIMARY KEY,
    agent_id      UUID                     NOT NULL UNIQUE,
    name          VARCHAR(100)             NOT NULL,
    platform      VARCHAR(100)             NOT NULL,
    agent_version VARCHAR(50)              NOT NULL,
    capabilities  JSONB                    NOT NULL DEFAULT '[]'::jsonb,
    registered_at TIMESTAMP WITH TIME ZONE NOT NULL,
    last_seen_at  TIMESTAMP WITH TIME ZONE NOT NULL
);

CREATE INDEX idx_devices_last_seen_at
    ON devices (last_seen_at);

CREATE TABLE device_commands
(
    id           UUID                     PRIMARY KEY,
    device_id    UUID                     NOT NULL,
    type         VARCHAR(100)             NOT NULL,
    parameters   JSONB                    NOT NULL DEFAULT '{}'::jsonb,
    status       VARCHAR(32)              NOT NULL,
    result       TEXT,
    error        TEXT,
    created_at   TIMESTAMP WITH TIME ZONE NOT NULL,
    started_at   TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE,

    CONSTRAINT fk_device_commands_device
        FOREIGN KEY (device_id)
            REFERENCES devices (id)
            ON DELETE CASCADE,

    CONSTRAINT ck_device_commands_status
        CHECK (
            status IN (
                       'PENDING',
                       'RUNNING',
                       'SUCCEEDED',
                       'FAILED'
                )
            )
);

CREATE INDEX idx_device_commands_device_status_created
    ON device_commands (device_id, status, created_at);

--rollback DROP TABLE device_commands;
--rollback DROP TABLE devices;
