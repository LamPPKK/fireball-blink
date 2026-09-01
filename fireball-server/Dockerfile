# Fireball Server - Multi-platform Headless Streaming & Sync Hub
FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1
ENV PORT=9090

# Install dependencies: Python 3, Chromium headless, XVFB, audio/video tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 \
    python3-pip \
    chromium \
    xvfb \
    ffmpeg \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy server sources
COPY . /app/

EXPOSE 9090

# Healthcheck
HEALTHCHECK --interval=30s --timeout=5s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:9090/health || exit 1

ENTRYPOINT ["python3", "beam_server.py", "--host", "0.0.0.0", "--port", "9090"]
