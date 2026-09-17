from python_services.ingestion_helpers.parsers.pdf_parser import extract_pdf_text


pages = extract_pdf_text("sample.pdf")

print("Number of pages:", len(pages))

for page in pages:
    print("\n--------------------")
    print("Page:", page["page_number"])
    print("Source:", page["source_location"])
    print("Text:")
    print(page["text"])