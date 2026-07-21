# R4 Linux Agent

`agent-linux` is a Kotlin/JVM application for Linux hosts. It communicates with R4 Hub over HTTP, retains a persistent `agentId`, sends heartbeats, polls for commands, and runs built-in command handlers.

Supported commands:

- `command.echo`
- `system.info`

## Requirements

- Linux with systemd
- x86_64/amd64 or aarch64/arm64
- Java 21 or newer
- `curl`, `tar`, `sha256sum`, and standard system administration tools
- an existing tagged GitHub Release for bootstrap installation

## Install

Install the latest release:

```bash
curl -fsSL https://raw.githubusercontent.com/Rarmash/r4-platform/main/agent-linux/install.sh \
  | sudo bash -s -- install \
      --hub-url "https://hub.example.com" \
      --name "server-01"
```

The name defaults to the system hostname when `--name` is omitted.

Install a specific release:

```bash
curl -fsSL https://raw.githubusercontent.com/Rarmash/r4-platform/main/agent-linux/install.sh \
  | sudo bash -s -- install \
      --hub-url "https://hub.example.com" \
      --version agent-v0.1.0
```

Versions in `0.1.0` form are normalized to `agent-v0.1.0`.

## Manage the Agent

Show the installed version, Hub URL, name, identity presence, and systemd state:

```bash
sudo r4-agentctl status
```

Update to the latest release or a specific release:

```bash
sudo r4-agentctl update
sudo r4-agentctl update --version agent-v0.1.0
```

An update preserves `/etc/r4-agent/r4-agent.env` and `/var/lib/r4-agent`, including `agent-id`. The existing application remains available until the new archive has been downloaded, verified, and unpacked. A failed service start restores the previous application.

Remove the application while retaining configuration and identity:

```bash
sudo r4-agentctl uninstall
```

Remove the application, configuration, identity, service account, and group:

```bash
sudo r4-agentctl uninstall --purge
```

For non-interactive execution, an intentional purge requires both `--purge` and `--yes`.

## Files

| Path | Purpose |
| --- | --- |
| `/opt/r4-agent` | application and installed `VERSION` |
| `/etc/r4-agent/r4-agent.env` | agent configuration |
| `/var/lib/r4-agent/agent-id` | persistent device identity |
| `/etc/systemd/system/r4-agent.service` | systemd unit |
| `/usr/local/sbin/r4-agentctl` | management command |

The application and unit are owned by `root:root`. The environment file is owned by `root:r4-agent` with mode `0640`. The agent runs as the unprivileged `r4-agent` user.

## Logs

```bash
journalctl -u r4-agent -f
```

## Configuration

| Variable | Default | Description |
| --- | --- | --- |
| `R4_HUB_URL` | `http://127.0.0.1:8080` | Hub base URL |
| `R4_AGENT_NAME` | system hostname during bootstrap | name registered with Hub |
| `R4_AGENT_DATA_DIR` | `/var/lib/r4-agent` | persistent identity directory |
| `R4_HEARTBEAT_INTERVAL` | `PT10S` | heartbeat interval |
| `R4_COMMAND_POLL_INTERVAL` | `PT5S` | command polling interval |
| `R4_HTTP_CONNECT_TIMEOUT` | `PT5S` | HTTP connection timeout |
| `R4_HTTP_REQUEST_TIMEOUT` | `PT10S` | HTTP request timeout |

Durations accept ISO-8601 values such as `PT10S` or integer milliseconds. Reinstallation and updates retain existing values and unknown additional variables. Explicit `--hub-url` and `--name` arguments update only their corresponding settings.

## Manual Installation

Build the application distribution:

```bash
./gradlew :agent-linux:installDist
```

Create the service account and directories:

```bash
sudo useradd --system --home-dir /var/lib/r4-agent --shell /usr/sbin/nologin r4-agent
sudo install -d -o root -g root -m 0755 /opt/r4-agent /etc/r4-agent
sudo install -d -o r4-agent -g r4-agent -m 0750 /var/lib/r4-agent
```

Install the application, configuration, and unit:

```bash
sudo cp -a agent-linux/build/install/agent-linux/. /opt/r4-agent/
sudo chown -R root:root /opt/r4-agent
sudo install -o root -g r4-agent -m 0640 \
  agent-linux/deploy/r4-agent.env.example /etc/r4-agent/r4-agent.env
sudo install -o root -g root -m 0644 \
  agent-linux/deploy/r4-agent.service /etc/systemd/system/r4-agent.service
sudo systemctl daemon-reload
sudo systemctl enable --now r4-agent
```

Edit `/etc/r4-agent/r4-agent.env` before starting the service when the Hub is not available at its default URL.

## Releases

Linux Agent releases use `agent-vX.Y.Z` Git tags. For example:

```bash
git tag agent-v0.2.0
git push origin agent-v0.2.0
```

The `Linux Agent` GitHub Actions workflow runs for pull requests and relevant pushes to `main`, but publishes a GitHub Release only for an `agent-vX.Y.Z` tag. The GitHub Release keeps the complete source tag as its name, while the archive's `VERSION` file contains only `X.Y.Z`.

Hub tags use the separate `hub-vX.Y.Z` namespace and do not trigger an Agent release.

## Release package

The release archive has this structure:

```text
r4-agent-linux/
├── app/
│   ├── bin/
│   └── lib/
├── deploy/
│   ├── r4-agent.service
│   └── r4-agent.env.example
├── install.sh
└── VERSION
```

The workflow tests the agent, validates the installer, builds this archive, verifies its checksum and structure, and attaches the archive and checksum to the matching Agent release.
