import pytest
from app.agent.agent import Agent
from app.schemas.models import AgentRequest, Identity, FailureResponse

def test_agent_run_stub():
    """Test the basic Agent.run signature."""
    agent = Agent()
    identity = Identity(user_id="user1", tenant_id="tenant1")
    request = AgentRequest(query="test", identity=identity)
    
    result = agent.run(request)
    assert isinstance(result, FailureResponse)
    assert result.query_id is not None
