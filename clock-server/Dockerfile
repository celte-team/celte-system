FROM celte-system-clock-server:latest

WORKDIR /app

# Copy full repo when building with project-root context so we can access .celte.yaml
COPY . /app

# Ensure the config from the repo root is available inside the image
COPY .celte.yaml /root/.celte.yaml
ENV CELTE_CONFIG=/root/.celte.yaml

WORKDIR /app/clock-server

RUN pip3 install --no-cache-dir -r requirements.txt

ENV PYTHONUNBUFFERED=1

CMD ["python3", "clock.py"]