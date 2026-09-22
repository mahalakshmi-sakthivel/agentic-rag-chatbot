from parsers.pdf_parser import extract_pdf_text
from cleaner import clean_pages


pages = extract_pdf_text("sample.pdf")

cleaned_pages = clean_pages(pages)

print("========== CLEANING RESULT ==========")
print("Number of pages:", len(cleaned_pages))


for page in cleaned_pages:

    print("\n--------------------")
    print("Page:", page["page_number"])
    print("Source:", page["source_location"])
    print("Cleaned text:")
    print(page["text"])