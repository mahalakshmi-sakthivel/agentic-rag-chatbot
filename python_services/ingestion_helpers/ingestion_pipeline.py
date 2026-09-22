from pathlib import Path

from validator import validate_file

from parsers.pdf_parser import extract_pdf_text
from parsers.csv_parser import extract_csv_text
from parsers.xlsx_parser import extract_xlsx_text
from parsers.json_parser import extract_json_text

from cleaner import clean_pages
from chunker import create_chunks
from metadata import add_metadata


def normalize_records(records: list[dict]) -> list[dict]:
    """
    Convert parser output from PDF/CSV/XLSX/JSON
    into a common page-like structure.

    The original source_location is preserved.
    """

    normalized = []

    for index, record in enumerate(records):

        normalized.append({
            "page_number": record.get(
                "page_number",
                record.get("row_number", index + 1)
            ),
            "text": record.get("text", ""),
            "source_location": record.get(
                "source_location",
                f"item {index + 1}"
            )
        })

    return normalized


def ingest_file(
    file_path: str,
    document_id: str = "test-document-001",
    tenant_id: str = "test-tenant-001",
    uploaded_by: str | None = None
) -> dict:
    """
    Complete Phase 3 ingestion pipeline.

    Supported formats:
    PDF, CSV, XLSX, JSON
    """

    path = Path(file_path)

    # --------------------------------------------------
    # STEP 1: Check file exists
    # --------------------------------------------------

    if not path.exists():

        return {
            "success": False,
            "error": f"File not found: {file_path}",
            "content": []
        }

    filename = path.name

    # --------------------------------------------------
    # STEP 2: Validate
    # --------------------------------------------------

    validation_result = validate_file(file_path)

    if not validation_result["valid"]:

        return {
            "success": False,
            "error": validation_result.get("message"),
            "validation": validation_result,
            "content": []
        }

    file_type = validation_result.get("file_type")

    # --------------------------------------------------
    # STEP 3: Parse
    # --------------------------------------------------

    try:

        if file_type == "pdf":

            records = extract_pdf_text(file_path)

        elif file_type == "csv":

            records = extract_csv_text(file_path)

        elif file_type == "xlsx":

            records = extract_xlsx_text(file_path)

        elif file_type == "json":

            records = extract_json_text(file_path)

        else:

            return {
                "success": False,
                "error": f"Unsupported file type: {file_type}",
                "validation": validation_result,
                "content": []
            }

    except Exception as error:

        return {
            "success": False,
            "error": str(error),
            "validation": validation_result,
            "content": []
        }

    # --------------------------------------------------
    # STEP 4: Normalize parser output
    # --------------------------------------------------

    normalized_records = normalize_records(records)

    # --------------------------------------------------
    # STEP 5: Clean
    # --------------------------------------------------

    cleaned_records = clean_pages(normalized_records)

    # --------------------------------------------------
    # STEP 6: Chunk
    # --------------------------------------------------

    chunks = create_chunks(
        pages=cleaned_records,
        document_id=document_id,
        tenant_id=tenant_id,
        filename=filename,
        uploaded_by=uploaded_by
    )

    # --------------------------------------------------
    # STEP 7: Add metadata
    # --------------------------------------------------

    final_chunks = add_metadata(
        chunks=chunks,
        filename=filename,
        file_type=file_type,
        uploaded_by=uploaded_by
    )

    # --------------------------------------------------
    # STEP 8: Final result
    # --------------------------------------------------

    return {
        "success": True,
        "filename": filename,
        "file_type": file_type,
        "document_id": document_id,
        "tenant_id": tenant_id,
        "validation": validation_result,
        "total_records": len(records),
        "total_chunks": len(final_chunks),
        "content": final_chunks
    }