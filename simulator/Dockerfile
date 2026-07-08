FROM eclipse-temurin:21-jdk-jammy AS build

WORKDIR /workspace

COPY . .

RUN chmod +x gradlew \
    && ./gradlew :simulator:bootJar --no-daemon


FROM eclipse-temurin:21-jre-jammy

WORKDIR /app

RUN useradd \
        --system \
        --uid 10001 \
        --home-dir /app \
        r4 \
    && mkdir -p /var/lib/r4 \
    && chown -R r4:r4 /app /var/lib/r4

COPY --from=build \
    /workspace/simulator/build/libs/r4-simulator.jar \
    /app/r4-simulator.jar

RUN chown r4:r4 /app/r4-simulator.jar

USER r4

ENTRYPOINT ["java", "-jar", "/app/r4-simulator.jar"]
