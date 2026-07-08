# R4 Linux Agent

`agent-linux` — обычное Kotlin/JVM-приложение для Linux-хостов. Агент работает с R4 Hub по существующему HTTP-протоколу, хранит постоянный `agentId`, отправляет heartbeat, запрашивает команды и выполняет только встроенные безопасные handlers.

Поддерживаемые команды:

- `command.echo`
- `system.info`

В этом milestone агент не предоставляет remote shell, OTA, управление Docker, MQTT или plugin runtime.

## Сборка

Используйте Java 21:

```bash
./gradlew :agent-linux:installDist
```

Дистрибутив приложения создаётся здесь:

```text
agent-linux/build/install/agent-linux
```

## Конфигурация

Конфигурация читается из environment variables:

| Variable | Default | Description |
| --- | --- | --- |
| `R4_HUB_URL` | `http://localhost:8080` | Hub base URL |
| `R4_AGENT_NAME` | local hostname | имя, регистрируемое в Hub |
| `R4_AGENT_DATA_DIR` | `/var/lib/r4-agent` on Linux | каталог для постоянного `agent-id` |
| `R4_HEARTBEAT_INTERVAL` | `PT10S` | интервал heartbeat |
| `R4_COMMAND_POLL_INTERVAL` | `PT5S` | интервал polling команд |
| `R4_HTTP_CONNECT_TIMEOUT` | `PT5S` | HTTP connect timeout |
| `R4_HTTP_REQUEST_TIMEOUT` | `PT10S` | HTTP request timeout |

Durations могут быть ISO-8601 значениями вроде `PT10S` или целыми миллисекундами.

## Ручная установка через systemd

Сначала соберите дистрибутив:

```bash
./gradlew :agent-linux:installDist
```

Создайте отдельного пользователя и каталоги:

```bash
sudo useradd --system --home-dir /var/lib/r4-agent --shell /usr/sbin/nologin r4-agent
sudo mkdir -p /opt/r4-agent /etc/r4-agent /var/lib/r4-agent
sudo chown r4-agent:r4-agent /var/lib/r4-agent
```

Скопируйте дистрибутив приложения:

```bash
sudo rsync -a --delete agent-linux/build/install/agent-linux/ /opt/r4-agent/
sudo chown -R root:root /opt/r4-agent
sudo chmod +x /opt/r4-agent/bin/agent-linux
```

Установите и отредактируйте environment-файл:

```bash
sudo cp agent-linux/deploy/r4-agent.env.example /etc/r4-agent/r4-agent.env
sudo editor /etc/r4-agent/r4-agent.env
sudo chown root:r4-agent /etc/r4-agent/r4-agent.env
sudo chmod 0640 /etc/r4-agent/r4-agent.env
```

Установите systemd unit:

```bash
sudo cp agent-linux/deploy/r4-agent.service /etc/systemd/system/r4-agent.service
sudo systemctl daemon-reload
sudo systemctl enable --now r4-agent
```

Посмотреть статус и логи:

```bash
systemctl status r4-agent
journalctl -u r4-agent -f
```

## Обновление

Остановите service, замените дистрибутив и снова запустите:

```bash
sudo systemctl stop r4-agent
sudo rsync -a --delete agent-linux/build/install/agent-linux/ /opt/r4-agent/
sudo chown -R root:root /opt/r4-agent
sudo chmod +x /opt/r4-agent/bin/agent-linux
sudo systemctl start r4-agent
```

`agentId` остаётся в `/var/lib/r4-agent/agent-id`.

## Удаление

```bash
sudo systemctl disable --now r4-agent
sudo rm -f /etc/systemd/system/r4-agent.service
sudo systemctl daemon-reload
sudo rm -rf /opt/r4-agent /etc/r4-agent
```

Удаляйте persistent identity только если намеренно хотите, чтобы при следующем запуске host зарегистрировался как новое устройство:

```bash
sudo rm -rf /var/lib/r4-agent
sudo userdel r4-agent
```
