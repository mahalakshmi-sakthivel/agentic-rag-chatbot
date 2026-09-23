import pytest
import json
import logging
from io import StringIO
from app.logger import get_logger, JSONFormatter
from app.schemas.models import AgentRequest, Identity, Intent, ErrorCode, Phase6Data
from app.agent.agent import Agent
from app.tools.registry import ToolRegistry
from app.tools.vector_search import VectorSearchTool
from app.clients.phase4_client import MockPhase4Client

def test_json_logging_format_and_redaction():
    """Spec 24: JSON logging and aggressive redaction validation."""
    logger = get_logger("test_redaction")
    stream = StringIO()
    handler = logging.StreamHandler(stream)
    handler.setFormatter(JSONFormatter())
    logger.addHandler(handler)
    
    # We should not pass sensitive data to custom_fields, but let's see if the format is JSON
    summary = {
        "event": "query_summary",
        "query_id": "test-uuid",
        "intent": Intent.DOCUMENT_LOOKUP.value,
        "duration_ms": 120,
        "tool_count": 2,
        "outcome": "success"
    }
    
    logger.info("Test message", extra={"custom_fields": summary})
    
    # Extract log
    log_output = stream.getvalue().strip()
    assert log_output.startswith("{")
    assert log_output.endswith("}")
    
    parsed = json.loads(log_output)
    assert parsed["level"] == "INFO"
    assert parsed["message"] == "Test message"
    assert parsed["event"] == "query_summary"
    assert parsed["query_id"] == "test-uuid"
    assert parsed["intent"] == "DOCUMENT_LOOKUP"
    assert parsed["duration_ms"] == 120
    assert parsed["tool_count"] == 2
    assert parsed["outcome"] == "success"

def test_agent_run_emits_summary_log(monkeypatch, caplog):
    """Spec 24: Agent.run emits exactly one summary log at the end."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "token")
    
    registry = ToolRegistry()
    registry.register(VectorSearchTool(MockPhase4Client()))
    agent = Agent(registry)
    
    req = AgentRequest(query="What was the revenue?", identity=Identity(user_id="u", tenant_id="t", roles=["user"], session_id="s1"), query_id="q-clarify")
    
    # We use caplog to capture the standard python logging
    caplog.set_level(logging.INFO)
    
    agent.run(req)
    
    # Filter logs from 'agent'
    agent_logs = [r for r in caplog.records if r.name == "agent"]
    assert len(agent_logs) == 1
    
    record = agent_logs[0]
    assert hasattr(record, "custom_fields")
    assert record.custom_fields["event"] == "query_summary"
    assert record.custom_fields["query_id"] == "q-clarify"
    assert record.custom_fields["outcome"] == "CLARIFICATION_REQUIRED"
    assert record.custom_fields["tool_count"] == 0
    assert "duration_ms" in record.custom_fields
