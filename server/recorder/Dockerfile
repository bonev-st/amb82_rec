FROM python:3.11-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends ffmpeg && \
    rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir paho-mqtt==2.1.0

COPY recorder.py /app/recorder.py

CMD ["python", "-u", "/app/recorder.py"]
