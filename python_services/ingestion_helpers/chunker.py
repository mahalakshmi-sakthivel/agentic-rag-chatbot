from uuid import uuid5, NAMESPACE_URL


def generate_stable_chunk_id(
    document_id: str,
    chunk_index: int
) -> str:
    """
    Generate a deterministic UUID for a chunk.

    The same document_id + chunk_index will always
    produce the same chunk_id.
    """

    return str(
        uuid5(
            NAMESPACE_URL,
            f"{document_id}-chunk-{chunk_index}"
        )
    )


def chunk_text(
    text: str,
    chunk_size: int = 4000,
    overlap: int = 800
) -> list[str]:
    """
    Split text into overlapping character-based chunks.

    NOTE:
    Final chunk/token size and overlap are TBD in the
    Phase 3 contract. These are temporary test values.
    """

    if not text or not text.strip():
        return []

    if chunk_size <= 0:
        raise ValueError("chunk_size must be greater than 0")

    if overlap < 0:
        raise ValueError("overlap cannot be negative")

    if overlap >= chunk_size:
        raise ValueError(
            "overlap must be smaller than chunk_size"
        )

    text = text.strip()

    chunks = []
    start = 0

    while start < len(text):

        end = min(
            start + chunk_size,
            len(text)
        )

        chunk = text[start:end].strip()

        if chunk:
            chunks.append(chunk)

        if end >= len(text):
            break

        start = end - overlap

    return chunks


def create_chunks(
    pages: list[dict],
    document_id: str,
    tenant_id: str,
    filename: str | None = None,
    uploaded_by: str | None = None,
    chunk_size: int = 4000,
    overlap: int = 800
) -> list[dict]:
    """
    Convert cleaned page-level text into Phase 3
    chunk objects.

    Page number and source location are preserved.
    """

    chunks = []
    chunk_index = 0

    for page in pages:

        page_text = page.get("text", "").strip()

        # Skip pages with no extractable content
        if not page_text:
            continue

        text_chunks = chunk_text(
            page_text,
            chunk_size=chunk_size,
            overlap=overlap
        )

        for text in text_chunks:

            stable_id = generate_stable_chunk_id(
                document_id,
                chunk_index
            )

            chunk = {
                "document_id": document_id,
                "tenant_id": tenant_id,
                "chunk_id": stable_id,
                "chunk_index": chunk_index,
                "text": text,
                "source_location": page["source_location"],
                "metadata": {}
            }

            if filename is not None:
                chunk["metadata"]["filename"] = filename

            if uploaded_by is not None:
                chunk["metadata"]["uploaded_by"] = uploaded_by

            chunks.append(chunk)

            chunk_index += 1

    return chunks