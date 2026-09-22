from parsers.csv_parser import extract_csv_text


rows = extract_csv_text("test.csv")

print("========== CSV PARSER RESULT ==========")
print("Number of rows:", len(rows))

for row in rows:
    print("\n--------------------")
    print("Row:", row["row_number"])
    print("Source:", row["source_location"])
    print("Text:", row["text"])