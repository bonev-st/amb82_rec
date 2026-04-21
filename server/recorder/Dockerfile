FROM python:3.11-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends ffmpeg && \
    rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir paho-mqtt==2.1.0

# Run as non-root. UID 1000 matches typical host-user ownership so the
# mounted ./clips volume stays writable without extra chown gymnastics.
# The entrypoint double-checks writability at startup and refuses to
# start with a clear error if the bind-mount is root-owned.
RUN useradd -u 1000 -m -s /usr/sbin/nologin recorder && \
    mkdir -p /clips && chown recorder:recorder /clips

COPY --chown=recorder:recorder recorder.py /app/recorder.py
COPY entrypoint.sh /entrypoint.sh
RUN chmod 0755 /entrypoint.sh

USER recorder

ENTRYPOINT ["/entrypoint.sh"]
CMD ["python", "-u", "/app/recorder.py"]
