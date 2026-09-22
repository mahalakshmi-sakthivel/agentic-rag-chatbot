from ingestion_pipeline import ingest_file


files = [
    "sample.pdf",
    "test.csv",
    "test.xlsx",
    "test.json"
]


for file in files:

    print("\n========================================")
    print("TESTING:", file)
    print("========================================")

    result = ingest_file(file)

    print("Success:", result["success"])

    if result["success"]:

        print("Filename:", result["filename"])
        print("File Type:", result["file_type"])
        print("Number of extracted items:", len(result["content"]))

        if result["content"]:
            print("\nFirst extracted item:")
            print(result["content"][0])

    else:

        print("Error:", result["error"])