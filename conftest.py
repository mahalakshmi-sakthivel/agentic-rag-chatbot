import os

os.environ.setdefault("INTERNAL_SERVICE_TOKEN", "test-secret-token")
os.environ.setdefault("LLM_PROVIDER", "mock")

import pytest
from fastapi.testclient import TestClient

from app.main import app


@pytest.fixture
def client():
    return TestClient(app)


@pytest.fixture
def valid_headers():
    return {
        "Content-Type": "application/json",
        "X-Request-ID": "req-test-1",
        "X-Internal-Auth": "test-secret-token",
    }


@pytest.fixture
def sample_context():
    # Same shape Phase 5's own test_real_clients.py sends and mocks against
    # (§19A.3) — kept in sync deliberately so both sides' tests exercise the
    # same wire format.
    return {
        "query_id": "q1",
        "identity": {
            "user_id": "u1",
            "tenant_id": "t1",
            "roles": ["user"],
            "session_id": "s1",
        },
        "original_query": "What was Q3 revenue?",
        "retrieved_chunks": [
            {
                "chunk_id": "c1",
                "document_id": "d1",
                "text": "Q3 revenue was 12.5 lakh rupees.",
                "score": 0.92,
                "source_location": "page 5",
                "filename": "annual_report.pdf",
            }
        ],
    }
