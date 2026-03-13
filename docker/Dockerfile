# ── Stage 1: Builder ────────────────────────────────────────────────────
FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y cmake && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt .
COPY include/       include/
COPY src/           src/
COPY config/        config/
COPY tests/         tests/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel

# ── Stage 2: Runtime ────────────────────────────────────────────────────
FROM gcc:13

WORKDIR /app

COPY --from=builder /app/build/TaskQueue .
COPY --from=builder /app/config/ config/

HEALTHCHECK --interval=5s --timeout=3s --start-period=5s --retries=3 \
    CMD grep -q "UP" /tmp/health || exit 1

CMD ["./TaskQueue"]