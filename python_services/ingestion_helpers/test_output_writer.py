from ingestion_pipeline import ingest_file
from output_writer import save_ingestion_output


result = ingest_file(
    "sample.pdf",
    document_id="test-document-001",
    tenant_id="test-tenant-001",
    uploaded_by="test-user-001"
)


if result["success"]:

    output_file = save_ingestion_output(result)

    print("========== OUTPUT WRITER ==========")
    print("Success: True")
    print("Output file:", output_file)

else:

    print("Ingestion failed")
    print("Error:", result["error"])