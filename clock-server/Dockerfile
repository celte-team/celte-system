FROM python:3.11-slim

# Set working directory for the clock server
WORKDIR /app/clock-server

# Install Python dependencies first to leverage Docker layer caching
COPY clock-server/requirements.txt /app/clock-server/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy the clock server source code
COPY clock-server/ /app/clock-server/

# Configuration is provided at runtime via volume in docker-compose
ENV CELTE_CONFIG=/root/.celte.yaml \
    PYTHONUNBUFFERED=1

CMD ["python", "clock.py"]