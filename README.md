# R4 Platform

R4 Platform manages devices and applications through a central Hub and lightweight agents.

## Modules

- `hub` - central Spring Boot service
- `protocol` - shared API contracts
- `simulator` - test device agent
- `agent-linux` - Linux host agent

## Requirements

- JDK 21

## Build

```bash
./gradlew clean check
```

On Windows:

```powershell
.\gradlew.bat clean check
```

## Run the Hub

```bash
./gradlew :hub:bootRun
```

## Run the Simulator

```bash
./gradlew :simulator:bootRun
```

## Install the Linux Agent

Java 21 and a systemd-based Linux distribution are required. A tagged GitHub Release must exist before running the installer.

```bash
curl -fsSL https://raw.githubusercontent.com/Rarmash/r4-platform/main/agent-linux/install.sh \
  | sudo bash -s -- install --hub-url "https://hub.example.com" --name "server-01"
```

See [agent-linux/README.md](agent-linux/README.md) for installation, updates, status, removal, and manual deployment.
