import json
from pathlib import Path


def save_ingestion_output(
    result: dict,
    output_directory: str = "ingestion_output"
) -> str:
    """
    Save the final Phase 3 ingestion result as JSON.
    """

    output_path = Path(output_directory)
    output_path.mkdir(parents=True, exist_ok=True)

    document_id = result.get(
        "document_id",
        "unknown-document"
    )

    file_path = output_path / f"{document_id}.json"

    with open(file_path, "w", encoding="utf-8") as file:

        json.dump(
            result,
            file,
            indent=4,
            ensure_ascii=False
        )

    return str(file_path)
