from validator import validate_pdf


result = validate_pdf(
    "sample.pdf",
    filename="sample.pdf",
    declared_mime_type="application/pdf"
)

print("Validation Result")
print("-----------------")

for key, value in result.items():
    print(f"{key}: {value}")