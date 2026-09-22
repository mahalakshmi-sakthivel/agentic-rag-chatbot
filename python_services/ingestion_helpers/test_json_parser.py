from parsers.json_parser import extract_json_text


rows = extract_json_text("test.json")

print("========== JSON PARSER RESULT ==========")
print("Number of items:", len(rows))

for row in rows:
    print("\n--------------------")
    print("Item:", row["row_number"])
    print("Source:", row["source_location"])
    print("Text:")
    print(row["text"])