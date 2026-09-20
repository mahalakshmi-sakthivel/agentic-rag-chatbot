from app.schemas.models import AgentState

def refine_query(state: AgentState) -> str:
    # Rule-based refinement
    query = state.refined_query or state.original_query
    
    # Very naive rule-based expansion for demonstration / test matching
    lower_q = query.lower()
    if "revenue" in lower_q and "q3" not in lower_q:
        return query + " total revenue in Q3"
        
    # Default refinement
    return query + " expanded details"
