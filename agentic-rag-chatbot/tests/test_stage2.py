import pytest
from app.tools.registry import ToolRegistry
from app.tools.calculator import CalculatorTool
from app.tools.vector_search import VectorSearchTool
from app.clients.phase4_client import MockPhase4Client, Phase4Request, Phase4Response
from app.schemas.models import Identity, ToolResult

def test_unknown_tool_rejected():
    """Spec 10: Unknown tools must be rejected."""
    registry = ToolRegistry()
    identity = Identity(user_id="u1", tenant_id="t1")
    res = registry.execute_tool("nonexistent", {"some": "data"}, identity)
    assert res.success is False
    assert res.error == "TOOL_NOT_FOUND"

def test_calculator_works():
    """Spec 13: Calculator tool works with allowed expressions."""
    calc = CalculatorTool()
    identity = Identity(user_id="u1", tenant_id="t1")
    
    # Normal math
    res = calc.execute({"expression": "(1250000 - 1000000) / 1000000 * 100"}, identity)
    assert res.success is True
    assert res.data["result"] == 25.0

    # Whitelisted functions
    res = calc.execute({"expression": "max(1, 2, 3) + abs(-5)"}, identity)
    assert res.success is True
    assert res.data["result"] == 8

def test_calculator_rejections():
    """Spec 13: Calculator tool rejects system(), exec(), imports, and huge exponents safely."""
    calc = CalculatorTool()
    identity = Identity(user_id="u1", tenant_id="t1")
    
    # 'import os' is not an expression
    res = calc.execute({"expression": "import os"}, identity)
    assert res.success is False
    assert "Syntax error" in res.error
    
    # system(...)
    res = calc.execute({"expression": "system('ls')"}, identity)
    assert res.success is False
    assert "Unsupported function" in res.error or "Variables are not allowed" in res.error

    # __import__
    res = calc.execute({"expression": "__import__('os').system('ls')"}, identity)
    assert res.success is False

    # exec
    res = calc.execute({"expression": "exec('x=1')"}, identity)
    assert res.success is False
    
    # huge exponent
    res = calc.execute({"expression": "2**1000"}, identity)
    assert res.success is False
    assert "Exponent too large" in res.error

    # very long input
    long_expr = "1 + " * 50 + "1"
    res = calc.execute({"expression": long_expr}, identity)
    assert res.success is False
    assert "Expression too long" in res.error

def test_vector_search_cannot_override_identity(monkeypatch):
    """Spec 11: Vector Search cannot override tenant_id and user_id."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    
    class SpyingMockClient:
        def search(self, request: Phase4Request) -> Phase4Response:
            self.last_request = request
            from app.clients.phase4_client import Phase4Result
            return Phase4Response(results=[])

    client = SpyingMockClient()
    tool = VectorSearchTool(client=client)
    identity = Identity(user_id="real_user", tenant_id="real_tenant")
    
    # Malicious input trying to override identity
    input_data = {
        "query_text": "test",
        "user_id": "fake_user",
        "tenant_id": "fake_tenant",
        "top_k": 5
    }
    
    res = tool.execute(input_data, identity)
    assert res.success is True
    
    # Check that identity was strictly enforced
    assert client.last_request.user_id == "real_user"
    assert client.last_request.tenant_id == "real_tenant"

def test_vector_search_rejects_top_k(monkeypatch):
    """Spec P1.1: Vector Search rejects invalid or excessively large top_k."""
    monkeypatch.setenv("PHASE6_BASE_URL", "http://localhost:8006")
    monkeypatch.setenv("INTERNAL_SERVICE_TOKEN", "secret-token")
    
    class SpyingMockClient:
        def search(self, request: Phase4Request) -> Phase4Response:
            self.last_request = request
            from app.clients.phase4_client import Phase4Result
            return Phase4Response(results=[])

    client = SpyingMockClient()
    tool = VectorSearchTool(client=client)
    identity = Identity(user_id="u", tenant_id="t")
    
    # Attempt to request 100 top_k (max is 20)
    input_data = {
        "query_text": "test",
        "top_k": 100
    }
    
    res = tool.execute(input_data, identity)
    assert res.success is False
    assert res.error == "TOOL_INPUT_INVALID"
