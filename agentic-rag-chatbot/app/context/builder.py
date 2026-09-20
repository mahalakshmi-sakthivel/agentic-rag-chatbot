from typing import List, Dict, Any
from app.schemas.models import AgentState, OrchestratorContext, Chunk, Step
from app.config import get_config

def build_context(state: AgentState) -> OrchestratorContext:
    config = get_config()
    
    # Dedupe by chunk_id
    seen_chunks = set()
    unique_chunks = []
    
    for chunk in state.retrieved_chunks:
        if chunk.chunk_id not in seen_chunks:
            # Drop below threshold
            if chunk.score >= config.MIN_RELEVANCE_SCORE:
                unique_chunks.append(chunk)
                seen_chunks.add(chunk.chunk_id)
                
    # Order by score descending
    unique_chunks.sort(key=lambda c: c.score, reverse=True)
    
    # Enforce MAX_TOP_K limit just in case
    unique_chunks = unique_chunks[:config.MAX_TOP_K]
    
    # Assemble context
    context = OrchestratorContext(
        query_id=state.query_id,
        identity=state.identity,
        original_query=state.original_query,
        retrieved_chunks=unique_chunks,
        refined_query=state.refined_query,
        steps_taken=state.steps_taken,
        conversation_history=state.conversation_history
    )
    
    if config.INCLUDE_TOOL_RESULTS and state.tool_results_list:
        context.tool_results = state.tool_results_list
        
    return context
