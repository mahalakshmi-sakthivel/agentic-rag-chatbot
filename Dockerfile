# Phase 6 — LLM Integration & Response Generation
FROM python:3.11-slim

WORKDIR /app

# PYTHONDONTWRITEBYTECODE: don't write .pyc files
# PYTHONUNBUFFERED: log straight to stdout/stderr without buffering
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PYTHONPATH=/app

# Copy packaging files first to leverage Docker layer caching
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app/ app/
COPY prompts/ prompts/

RUN groupadd --system app && useradd --system --gid app --no-create-home app \
    && chown -R app:app /app
USER app

EXPOSE 8006

HEALTHCHECK --interval=10s --timeout=5s --start-period=5s --retries=5 \
  CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8006/health')" || exit 1

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8006"]
