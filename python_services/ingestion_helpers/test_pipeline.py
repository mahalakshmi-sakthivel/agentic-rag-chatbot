from ingestion_pipeline import ingest_pdf


result = ingest_pdf("sample.pdf")

print("========== INGESTION RESULT ==========")
print("Success:", result["success"])

print("\n========== VALIDATION ==========")
print(result["validation"])

print("\n========== PARSED PAGES ==========")
print("Number of pages:", len(result["pages"]))

for page in result["pages"][:3]:
    print("\n--------------------")
    print("Page:", page["page_number"])
    print("Source:", page["source_location"])
    print("Text preview:", page["text"][:300])