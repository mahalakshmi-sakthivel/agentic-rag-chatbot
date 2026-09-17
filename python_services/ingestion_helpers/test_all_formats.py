from validator import validate_file


files = [
    "sample.pdf",
    "test.csv",
    "test.xlsx",
    "test.json"
]


for file in files:

    print("\n==============================")
    print("Testing:", file)
    print("==============================")

    result = validate_file(file)

    print("Valid:", result.get("valid"))
    print("Error Code:", result.get("error_code"))
    print("Message:", result.get("message"))
    print("File Type:", result.get("file_type"))

    if "page_count" in result:
        print("Page Count:", result["page_count"])

    if "sheet_count" in result:
        print("Sheet Count:", result["sheet_count"])