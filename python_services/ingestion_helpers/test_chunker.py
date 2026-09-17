from parsers.pdf_parser import extract_pdf_text
from cleaner import clean_pages
from chunker import create_chunks


# Step 1: Parse PDF
pages = extract_pdf_text("sample.pdf")

# Step 2: Clean extracted text
cleaned_pages = clean_pages(pages)

# Step 3: Create chunks
chunks = create_chunks(
    pages=cleaned_pages,
    document_id="test-document-001",
    tenant_id="test-tenant-001",
    filename="sample.pdf",
    uploaded_by="test-user-001",
    chunk_size=4000,
    overlap=800
)

# Display result
print("========== CHUNKING RESULT ==========")
print("Total chunks:", len(chunks))

for chunk in chunks:

    print("\n--------------------")
    print("Document ID:", chunk["document_id"])
    print("Tenant ID:", chunk["tenant_id"])
    print("Chunk ID:", chunk["chunk_id"])
    print("Chunk Index:", chunk["chunk_index"])
    print("Source:", chunk["source_location"])
    print("Metadata:", chunk["metadata"])
    print("Text:")
    print(chunk["text"][:500])