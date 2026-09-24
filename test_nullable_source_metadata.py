# tests/test_nullable_source_metadata.py
#
# Verifies that Phase 6 safely accepts nullable source metadata (source_location
# and filename as None/null) from Phase 5 / Phase 4 retrieval, does not invent
# placeholder strings, preserves trusted document_id and chunk_id, and returns
# page_or_location as None in sources.


def test_generate_with_null_source_metadata(client, valid_headers):
    payload = {
        "query_id": "query-null-meta-1",
        "identity": {
            "user_id": "user-1",
            "tenant_id": "tenant-1",
            "roles": ["user"],
            "session_id": "session-1",
        },
        "original_query": "What is the content?",
        "retrieved_chunks": [
            {
                "chunk_id": "chunk-1",
                "document_id": "doc-1",
                "text": "Example document content.",
                "score": 0.91,
                "source_location": None,
                "filename": None,
            }
        ],
    }

    resp = client.post("/internal/v1/llm/generate", json=payload, headers=valid_headers)
    assert resp.status_code == 200
    body = resp.json()

    # 1. Request and envelope validation succeeds
    assert body["success"] is True
    data = body["data"]

    # 2. LLM generation succeeds using existing mock/test mechanism
    assert isinstance(data["answer"], str)
    assert len(data["answer"]) > 0

    # 3. Validation succeeds according to the existing test setup
    validation = data["validation"]
    assert validation["passed"] is True

    # 4. Response does not invent source metadata
    # 5. document_id remains "doc-1"
    # 6. chunk_id remains "chunk-1"
    # 7. page_or_location remains null / None
    assert len(data["sources"]) == 1
    source = data["sources"][0]
    assert source["document_id"] == "doc-1"
    assert source["chunk_id"] == "chunk-1"
    assert source["page_or_location"] is None
    assert source["score"] == 0.91
    assert "Example document content." in source["excerpt"]
