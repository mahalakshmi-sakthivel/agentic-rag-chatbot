import re
import unicodedata


def normalize_whitespace(text: str) -> str:
    """
    Normalize redundant whitespace in extracted text.
    """

    # Replace tabs and multiple spaces with a single space
    text = re.sub(r"[ \t]+", " ", text)

    # Remove excessive blank lines
    text = re.sub(r"\n\s*\n+", "\n\n", text)

    # Remove spaces at the beginning/end of lines
    text = "\n".join(line.strip() for line in text.splitlines())

    return text.strip()


def fix_encoding(text: str) -> str:
    """
    Normalize text to consistent Unicode representation.
    """

    return unicodedata.normalize("NFC", text)


def remove_boilerplate(
    pages: list[dict],
    min_repetitions: int = 2
) -> list[dict]:
    """
    Remove repeated lines that appear across multiple pages.

    Repeated lines are treated as possible non-content
    boilerplate such as headers or footers.
    """

    line_counts = {}

    # Count how many pages contain each line
    for page in pages:

        lines = set(
            line.strip()
            for line in page["text"].splitlines()
            if line.strip()
        )

        for line in lines:
            line_counts[line] = line_counts.get(line, 0) + 1

    # Lines repeated across multiple pages are candidates
    # for boilerplate removal
    repeated_lines = {
        line
        for line, count in line_counts.items()
        if count >= min_repetitions
    }

    cleaned_pages = []

    for page in pages:

        cleaned_lines = []

        for line in page["text"].splitlines():

            stripped_line = line.strip()

            if stripped_line and stripped_line not in repeated_lines:
                cleaned_lines.append(stripped_line)

        cleaned_text = "\n".join(cleaned_lines)

        cleaned_pages.append({
            "page_number": page["page_number"],
            "text": cleaned_text,
            "source_location": page["source_location"]
        })

    return cleaned_pages


def clean_pages(pages: list[dict]) -> list[dict]:
    """
    Clean page-level extracted text while preserving
    page number and source location.
    """

    cleaned_pages = []

    for page in pages:

        text = page["text"]

        # 1. Encoding normalization
        text = fix_encoding(text)

        # 2. Whitespace normalization
        text = normalize_whitespace(text)

        cleaned_pages.append({
            "page_number": page["page_number"],
            "text": text,
            "source_location": page["source_location"]
        })

    # 3. Remove repeated boilerplate
    cleaned_pages = remove_boilerplate(cleaned_pages)

    return cleaned_pages