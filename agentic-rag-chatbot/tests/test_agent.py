import pytest
from app.agent.agent import Agent
from app.schemas.models import AgentRequest, Identity, FailureResponse

from app.tools.registry import ToolRegistry

def test_agent_run_stub():
    """Spec 19 / Inbound Interface: Test the basic Agent.run signature and AgentResult shape."""
    registry = ToolRegistry()
    agent = Agent(registry=registry)
    identity = Identity(user_id="user1", tenant_id="tenant1", roles=["user"], session_id="s1")
    request = AgentRequest(query="test", identity=identity)
    
    result = agent.run(request)
    assert isinstance(result, FailureResponse) # With empty registry, validation or execution might fail
    assert result.query_id is not None
