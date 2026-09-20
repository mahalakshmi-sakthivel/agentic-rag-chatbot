import re
from app.schemas.models import Intent

def detect_intent(query: str) -> Intent:
    lower_query = query.lower()
    
    if lower_query.endswith("?"):
        # Very short queries with missing context might need clarification
        words = lower_query.split()
        if len(words) <= 4 and re.search(r'\b(what|who|where|when|why)\b', lower_query):
            if "revenue" in lower_query or "profit" in lower_query:
                 return Intent.CLARIFICATION_REQUIRED
                 
    # Rule-based detection using regex/keywords
    if re.search(r'\b(compare|difference|vs|versus)\b', lower_query):
        if re.search(r'\b(and calculate|then calculate|percentage)\b', lower_query):
            return Intent.MULTI_STEP
        return Intent.COMPARISON
        
    if (re.search(r'\b(calculate|percent|sum|average|add|subtract|multiply|divide)\b', lower_query) or '%' in lower_query) and re.search(r'\d', lower_query):
        return Intent.CALCULATION
        
    if re.search(r'\b(revenue|highest|lowest|total|sales|monthly|quarterly|q1|q2|q3|q4)\b', lower_query) and re.search(r'\b(was|is|did|highest|lowest)\b', lower_query):
        # We classify structured data query when it asks for specific financial/metric figures
        return Intent.STRUCTURED_DATA_QUERY
        
    if re.search(r'\b(document|pdf|file|uploaded|policy|say about|mention)\b', lower_query):
        return Intent.DOCUMENT_LOOKUP

    # Default fallback to document lookup
    return Intent.DOCUMENT_LOOKUP
