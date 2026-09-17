from python_services.ingestion_helpers.parsers.pdf_parser import extract_pdf_text
from cleaner import clean_pages
from chunker import create_chunks
from metadata import add_metadata


# 1. Parse PDF
pages = extract_pdf_text("sample.pdf")

# 2. Clean PDF text
cleaned_pages = clean_pages(pages)

# 3. Create chunks
chunks = create_chunks(
    cleaned_pages,
    document_id="test-document-001",
    tenant_id="test-tenant-001",
    filename="sample.pdf",
    uploaded_by="test-user-001"
)

# 4. Add metadata
final_chunks = add_metadata(
    chunks,
    filename="sample.pdf",
    file_type="pdf",
    uploaded_by="test-user-001"
)


print("========== METADATA RESULT ==========")
print("Total chunks:", len(final_chunks))


for chunk in final_chunks[:3]:

    print("\n--------------------")
    print("Document ID:", chunk["document_id"])
    print("Tenant ID:", chunk["tenant_id"])
    print("Chunk ID:", chunk["chunk_id"])
    print("Chunk Index:", chunk["chunk_index"])
    print("Source:", chunk["source_location"])
    print("Metadata:", chunk["metadata"])
    print("Text:")
    print(chunk["text"][:300])