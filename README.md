# R4 Platform

Платформа управления устройствами и приложениями.

## Modules

- `hub` — центральный Spring Boot-сервис
- `protocol` — общие API-контракты
- `simulator` — тестовый агент устройства

## Requirements

- JDK 21

## Build

```powershell
.\gradlew.bat clean check
```

## Run Hub

```powershell
.\gradlew.bat :hub:bootRun
```

## Run Simulator

```powershell
.\gradlew.bat :simulator:bootRun
```
