import sys
import json
from pathlib import Path

from validator import validate_pdf
from python_services.ingestion_helpers.parsers.pdf_parser import extract_pdf_text
from cleaner import clean_pages
from chunker import create_chunks
from metadata import add_metadata


def run_ingestion(file_path: str) -> dict:
    """
    Run the complete Phase 3 PDF ingestion pipeline.

    Flow:
    Validation → Parsing → Cleaning → Chunking → Metadata
    """

    pdf_path = Path(file_path)

    # -------------------------------------------------
    # 1. VALIDATION
    # -------------------------------------------------

    validation_result = validate_pdf(
        str(pdf_path),
        filename=pdf_path.name,
        declared_mime_type="application/pdf"
    )

    if not validation_result["valid"]:
        return {
            "success": False,
            "stage": "validation",
            "error": validation_result
        }

    print("✓ Validation successful")


    # -------------------------------------------------
    # 2. PDF PARSING
    # -------------------------------------------------

    pages = extract_pdf_text(str(pdf_path))

    print(f"✓ Parsing successful: {len(pages)} pages")


    # -------------------------------------------------
    # 3. CLEANING
    # -------------------------------------------------

    cleaned_pages = clean_pages(pages)

    print("✓ Cleaning successful")


    # -------------------------------------------------
    # 4. CHUNKING
    # -------------------------------------------------

    chunks = create_chunks(
        cleaned_pages,
        document_id="test-document-001",
        tenant_id="test-tenant-001",
        filename=pdf_path.name,
        uploaded_by="test-user-001"
    )

    print(f"✓ Chunking successful: {len(chunks)} chunks")


    # -------------------------------------------------
    # 5. METADATA
    # -------------------------------------------------

    final_chunks = add_metadata(
        chunks,
        filename=pdf_path.name,
        file_type="pdf",
        uploaded_by="test-user-001"
    )

    print("✓ Metadata added")


    # -------------------------------------------------
    # FINAL RESULT
    # -------------------------------------------------

    return {
        "success": True,
        "validation": validation_result,
        "chunks": final_chunks
    }


if __name__ == "__main__":

    if len(sys.argv) != 2:
        print("Usage:")
        print("python run_ingestion.py <pdf_file>")
        sys.exit(1)

    pdf_file = sys.argv[1]

    result = run_ingestion(pdf_file)

    print("\n========== FINAL RESULT ==========")

    print("Success:", result["success"])

    if not result["success"]:
        print("Failed stage:", result["stage"])
        print("Error:", result["error"])
        sys.exit(1)

    print("Total chunks:", len(result["chunks"]))

    print("\n========== FIRST CHUNK ==========")

    if result["chunks"]:
        print(
            json.dumps(
                result["chunks"][0],
                indent=4
            )
        )