from app.tools.structured_query import StructuredQueryTool, MockStructuredQueryBackend
from app.schemas.models import Identity

def test_structured_query_works():
    backend = MockStructuredQueryBackend()
    tool = StructuredQueryTool(backend=backend)
    identity = Identity(user_id="u1", tenant_id="t1", roles=["user"], session_id="s1")
    
    res = tool.execute({"query": "What is revenue?"}, identity)
    assert res.success is True
    assert "Mocked structured data" in res.data["result"]
