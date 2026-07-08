# R4 Deployment

This document covers the Hub Docker image, the production Compose stack, and Docker Hub publication.

## Hub Image

Build the Hub image from the repository root:

```bash
docker build -f hub/Dockerfile -t r4-hub:local .
```

The Dockerfile uses the Gradle Wrapper to build `:hub:bootJar`. The runtime image runs `/app/r4-hub.jar` as a non-root user and accepts JVM options through `JAVA_OPTS`.

## Production Compose

Create the environment file:

```bash
cp .env.example .env
```

Set at least these values in `.env`:

```text
R4_HUB_VERSION=0.1.0
R4_DB_PASSWORD=<strong-password>
```

Validate and start the stack:

```bash
docker compose -f compose.production.yml --env-file .env config
docker compose -f compose.production.yml --env-file .env up -d
```

Update the stack after publishing a new image:

```bash
docker compose -f compose.production.yml --env-file .env pull
docker compose -f compose.production.yml --env-file .env up -d
```

The Compose file does not use `latest` as its production default. Set `R4_HUB_VERSION` to a release version such as `0.1.0` or an immutable short Git SHA tag.

## Docker Hub Publication

The `.github/workflows/docker-hub.yml` workflow uses:

- the `DOCKERHUB_USERNAME` GitHub Actions variable or secret;
- the `DOCKERHUB_TOKEN` GitHub Actions secret.

Use a Docker Hub access token rather than an account password.

Publication behavior:

- pull requests run tests and build the image without pushing it;
- pushes to `main` publish `edge` and a short Git SHA tag;
- Git tags in `vX.Y.Z` form publish semantic version tags and `latest`.

The Hub image targets `linux/amd64`.
