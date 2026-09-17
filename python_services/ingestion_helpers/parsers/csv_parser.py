import csv
from pathlib import Path


def extract_csv_text(file_path: str) -> list[dict]:
    """
    Extract text from a CSV file.

    Returns:
        A list containing row ranges, extracted text,
        and source location.
    """

    csv_path = Path(file_path)

    if not csv_path.exists():
        raise FileNotFoundError(f"CSV file not found: {file_path}")

    rows = []

    with open(csv_path, "r", encoding="utf-8-sig", newline="") as file:
        reader = csv.reader(file)

        for row_number, row in enumerate(reader, start=1):

            # Skip completely empty rows
            if not any(cell.strip() for cell in row):
                continue

            text = " | ".join(cell.strip() for cell in row)

            rows.append({
                "row_number": row_number,
                "text": text,
                "source_location": f"row {row_number}"
            })

    return rows