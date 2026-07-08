# Развёртывание R4

Документ описывает поставочные части Milestone 0.3: Docker-образ Hub, production-подобный Compose и публикацию в Docker Hub.

## Образ Hub

Собрать образ Hub локально из корня репозитория:

```bash
docker build -f hub/Dockerfile -t r4-hub:local .
```

Dockerfile использует Gradle Wrapper и собирает `:hub:bootJar`. Runtime-образ запускает `/app/r4-hub.jar` не от root и принимает JVM options через `JAVA_OPTS`.

## Production Compose

Создать environment-файл:

```bash
cp .env.example .env
```

Отредактировать `.env` и задать как минимум:

```text
R4_HUB_VERSION=0.1.0
R4_DB_PASSWORD=<strong-password>
```

Запустить Hub и PostgreSQL:

```bash
docker compose -f compose.production.yml --env-file .env up -d
```

Проверить конфигурацию без запуска контейнеров:

```bash
docker compose -f compose.production.yml --env-file .env config
```

Обновить сервер после публикации нового образа:

```bash
docker compose -f compose.production.yml --env-file .env pull
docker compose -f compose.production.yml --env-file .env up -d
```

Compose-файл не использует `latest` как production default. Закрепляйте `R4_HUB_VERSION` на release tag вроде `0.1.0` или на неизменяемом коротком Git SHA tag.

## Публикация в Docker Hub

Workflow `.github/workflows/docker-hub.yml` ожидает:

- GitHub Actions variable `DOCKERHUB_USERNAME` или secret `DOCKERHUB_USERNAME`;
- GitHub Actions secret `DOCKERHUB_TOKEN`.

Используйте Docker Hub access token, а не пароль аккаунта.

Правила публикации:

- pull request: запустить тесты, собрать `:hub:bootJar`, собрать Docker image, не выполнять push;
- push в `main`: опубликовать `edge` и короткий Git SHA tag;
- Git tag `vX.Y.Z`: опубликовать semver tags и `latest`.

В этом milestone собирается только `linux/amd64`. Если Hub позже потребуется запускать напрямую на ARM-устройстве вроде Orange Pi, нужно добавить отдельный ARM64 build path.
