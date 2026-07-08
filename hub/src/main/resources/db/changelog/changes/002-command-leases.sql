--liquibase formatted sql

--changeset rarmash:002-command-leases

ALTER TABLE device_commands
    ADD COLUMN attempt_count INTEGER NOT NULL DEFAULT 0,
    ADD COLUMN lease_expires_at TIMESTAMP WITH TIME ZONE,
    ADD COLUMN lease_token UUID;

ALTER TABLE device_commands
    ADD CONSTRAINT ck_device_commands_attempt_count
        CHECK (attempt_count >= 0);

CREATE INDEX idx_device_commands_expired_lease
    ON device_commands (
                        device_id,
                        lease_expires_at,
                        created_at
        )
    WHERE status = 'RUNNING';

--rollback DROP INDEX idx_device_commands_expired_lease;
--rollback ALTER TABLE device_commands DROP CONSTRAINT ck_device_commands_attempt_count;
--rollback ALTER TABLE device_commands DROP COLUMN lease_token;
--rollback ALTER TABLE device_commands DROP COLUMN lease_expires_at;
--rollback ALTER TABLE device_commands DROP COLUMN attempt_count;
