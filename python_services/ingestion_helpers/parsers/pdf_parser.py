from pathlib import Path
from pypdf import PdfReader


def extract_pdf_text(file_path: str) -> list[dict]:
    """
    Extract text from each page of a PDF.

    Returns:
        A list containing page number, extracted text,
        and source location.
    """

    pdf_path = Path(file_path)

    if not pdf_path.exists():
        raise FileNotFoundError(f"PDF not found: {file_path}")

    reader = PdfReader(str(pdf_path))

    pages = []

    for page_number, page in enumerate(reader.pages, start=1):
        text = page.extract_text() or ""

        pages.append({
            "page_number": page_number,
            "text": text,
            "source_location": f"page {page_number}"
        })

    return pages