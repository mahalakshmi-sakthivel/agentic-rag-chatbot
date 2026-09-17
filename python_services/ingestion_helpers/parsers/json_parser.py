import json
from pathlib import Path


def extract_json_text(file_path: str) -> list[dict]:
    """
    Extract JSON content and convert it into
    a consistent row-based structure.
    """

    json_path = Path(file_path)

    if not json_path.exists():
        raise FileNotFoundError(f"JSON file not found: {file_path}")

    with open(json_path, "r", encoding="utf-8") as file:
        data = json.load(file)

    rows = []

    if isinstance(data, list):
        items = data

    elif isinstance(data, dict):
        items = [data]

    else:
        items = [{"value": data}]

    for index, item in enumerate(items, start=1):

        text = json.dumps(
            item,
            ensure_ascii=False,
            indent=2
        )

        rows.append({
            "row_number": index,
            "text": text,
            "source_location": f"json item {index}"
        })

    return rows