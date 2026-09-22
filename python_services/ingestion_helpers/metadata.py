def add_metadata(
    chunks: list[dict],
    filename: str,
    file_type: str = "pdf",
    uploaded_by: str | None = None
) -> list[dict]:
    """
    Add document metadata to each chunk.

    Existing Phase 3 → Phase 4 fields are preserved.
    """

    enriched_chunks = []

    for chunk in chunks:

        metadata = dict(chunk.get("metadata", {}))

        # Required document information
        metadata["filename"] = filename
        metadata["file_type"] = file_type

        # Add uploader only when available
        if uploaded_by is not None:
            metadata["uploaded_by"] = uploaded_by

        enriched_chunk = {
            "document_id": chunk["document_id"],
            "tenant_id": chunk["tenant_id"],
            "chunk_id": chunk["chunk_id"],
            "chunk_index": chunk["chunk_index"],
            "text": chunk["text"],
            "source_location": chunk["source_location"],
            "metadata": metadata
        }

        enriched_chunks.append(enriched_chunk)

    return enriched_chunks