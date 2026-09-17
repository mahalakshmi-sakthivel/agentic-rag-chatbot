from ingestion_pipeline import ingest_file


files = [
    "sample.pdf",
    "test.csv",
    "test.xlsx",
    "test.json"
]

for file_path in files:

    print("\n" + "=" * 50)
    print("TESTING:", file_path)
    print("=" * 50)

    result = ingest_file(
        file_path,
        document_id="test-document-001",
        tenant_id="test-tenant-001",
        uploaded_by="test-user-001"
    )

    print("Success:", result["success"])
    print("File Type:", result.get("file_type"))
    print("Total Records:", len(result.get("records", [])))
    print("Total Chunks:", len(result.get("chunks", [])))

    if result["success"] and result.get("chunks"):
        print("\nFirst Chunk:")
        print(result["chunks"][0])