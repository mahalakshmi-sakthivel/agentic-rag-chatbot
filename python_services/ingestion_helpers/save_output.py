import json
from pathlib import Path

from run_ingestion import run_ingestion


def save_ingestion_output(pdf_file: str, output_file: str):
    result = run_ingestion(pdf_file)

    if not result["success"]:
        print("Ingestion failed.")
        print(result["error"])
        return

    output_path = Path(output_file)

    with output_path.open("w", encoding="utf-8") as file:
        json.dump(
            result["chunks"],
            file,
            indent=4,
            ensure_ascii=False
        )

    print("\n========== OUTPUT SAVED ==========")
    print("File:", output_path)
    print("Total chunks:", len(result["chunks"]))


if __name__ == "__main__":
    save_ingestion_output(
        "sample.pdf",
        "ingestion_output.json"
    )