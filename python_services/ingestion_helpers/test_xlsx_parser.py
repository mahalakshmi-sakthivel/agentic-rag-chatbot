from parsers.xlsx_parser import extract_xlsx_text


rows = extract_xlsx_text("test.xlsx")

print("========== XLSX PARSER RESULT ==========")
print("Number of rows:", len(rows))

for row in rows:
    print("\n--------------------")
    print("Sheet:", row.get("sheet_name", "Unknown"))
    print("Row:", row.get("row_number", "Unknown"))
    print("Source:", row.get("source_location", "Unknown"))
    print("Text:", row.get("text", ""))