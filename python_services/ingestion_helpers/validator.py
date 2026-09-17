from pathlib import Path
import json


# ============================================================
# SUPPORTED FILE TYPES
# ============================================================

SUPPORTED_EXTENSIONS = {
    ".pdf": "pdf",
    ".csv": "csv",
    ".xlsx": "xlsx",
    ".json": "json"
}


# ============================================================
# EXPECTED MIME TYPES
# ============================================================

EXPECTED_MIME_TYPES = {
    "pdf": "application/pdf",
    "csv": "text/csv",
    "xlsx": "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    "json": "application/json"
}


# ============================================================
# MAIN VALIDATOR
# ============================================================

def validate_file(
    file_path: str,
    filename: str | None = None,
    declared_mime_type: str | None = None,
    max_size_bytes: int | None = None
) -> dict:
    """
    Validate a Phase 3 input file.

    Supported formats:
        PDF
        CSV
        XLSX
        JSON

    Validation checks:
        1. File exists
        2. Filename/path safety
        3. Supported file extension
        4. File is not empty
        5. Maximum file size, if configured
        6. MIME type, if provided
        7. Format-specific validation
    """

    # --------------------------------------------------------
    # 1. CHECK FILE EXISTS
    # --------------------------------------------------------

    path = Path(file_path)

    if not path.exists():

        return {
            "valid": False,
            "error_code": "FILE_NOT_FOUND",
            "message": "The uploaded file does not exist."
        }


    # --------------------------------------------------------
    # 2. CHECK THAT IT IS A FILE
    # --------------------------------------------------------

    if not path.is_file():

        return {
            "valid": False,
            "error_code": "NOT_A_FILE",
            "message": "The provided path is not a file."
        }


    # --------------------------------------------------------
    # 3. FILENAME SAFETY
    # --------------------------------------------------------

    safe_filename = filename or path.name

    # Empty or special filenames
    if safe_filename in {"", ".", ".."}:

        return {
            "valid": False,
            "error_code": "UNSAFE_FILENAME",
            "message": "The filename is invalid."
        }


    # Prevent path injection / traversal
    if "/" in safe_filename or "\\" in safe_filename:

        return {
            "valid": False,
            "error_code": "UNSAFE_FILENAME",
            "message": "Path separators are not allowed in filenames."
        }


    if ".." in safe_filename:

        return {
            "valid": False,
            "error_code": "UNSAFE_FILENAME",
            "message": "Path traversal pattern is not allowed."
        }


    # --------------------------------------------------------
    # 4. DETERMINE FILE TYPE
    # --------------------------------------------------------

    extension = path.suffix.lower()

    if extension not in SUPPORTED_EXTENSIONS:

        return {
            "valid": False,
            "error_code": "UNSUPPORTED_FILE_TYPE",
            "message": (
                "Unsupported file type. "
                "Supported formats are PDF, CSV, XLSX and JSON."
            )
        }


    file_type = SUPPORTED_EXTENSIONS[extension]


    # --------------------------------------------------------
    # 5. CHECK FILE SIZE
    # --------------------------------------------------------

    size_bytes = path.stat().st_size

    if size_bytes == 0:

        return {
            "valid": False,
            "error_code": "EMPTY_FILE",
            "message": "The uploaded file is empty.",
            "filename": safe_filename,
            "file_type": file_type,
            "size_bytes": size_bytes
        }


    # Maximum size is optional because the final limit
    # is still TBD in the Phase 3 contract.

    if max_size_bytes is not None:

        if size_bytes > max_size_bytes:

            return {
                "valid": False,
                "error_code": "FILE_TOO_LARGE",
                "message": (
                    "The file exceeds the configured "
                    "maximum file size."
                ),
                "filename": safe_filename,
                "file_type": file_type,
                "size_bytes": size_bytes,
                "max_size_bytes": max_size_bytes
            }


    # --------------------------------------------------------
    # 6. MIME TYPE VALIDATION
    # --------------------------------------------------------

    if declared_mime_type is not None:

        expected_mime = EXPECTED_MIME_TYPES[file_type]

        if declared_mime_type.lower() != expected_mime.lower():

            return {
                "valid": False,
                "error_code": "INVALID_MIME_TYPE",
                "message": (
                    "Declared MIME type does not match "
                    "the file type."
                ),
                "filename": safe_filename,
                "file_type": file_type,
                "declared_mime_type": declared_mime_type,
                "expected_mime_type": expected_mime
            }


    # --------------------------------------------------------
    # 7. FORMAT-SPECIFIC VALIDATION
    # --------------------------------------------------------

    if file_type == "pdf":

        return _validate_pdf_content(
            path,
            safe_filename,
            size_bytes
        )


    if file_type == "csv":

        return validate_csv(
            path,
            safe_filename,
            size_bytes
        )


    if file_type == "xlsx":

        return validate_xlsx(
            path,
            safe_filename,
            size_bytes
        )


    if file_type == "json":

        return validate_json(
            path,
            safe_filename,
            size_bytes
        )


    # This should never be reached.

    return {
        "valid": False,
        "error_code": "VALIDATION_ERROR",
        "message": "Unknown validation error."
    }


# ============================================================
# PDF VALIDATION
# ============================================================

def _validate_pdf_content(
    file_path: Path,
    filename: str,
    size_bytes: int
) -> dict:
    """
    Validate PDF file.

    Checks:
        - PDF magic bytes
        - PDF readability
        - Page count
    """

    # --------------------------------------------------------
    # Check PDF magic bytes
    # --------------------------------------------------------

    try:

        with file_path.open("rb") as file:

            header = file.read(4)

    except Exception as error:

        return {
            "valid": False,
            "error_code": "FILE_READ_ERROR",
            "message": "Unable to read the PDF file.",
            "details": str(error)
        }


    if header != b"%PDF":

        return {
            "valid": False,
            "error_code": "INVALID_FILE_SIGNATURE",
            "message": "The file does not have a valid PDF signature.",
            "filename": filename,
            "file_type": "pdf"
        }


    # --------------------------------------------------------
    # Check PDF readability
    # --------------------------------------------------------

    try:

        from pypdf import PdfReader

        reader = PdfReader(str(file_path))

        page_count = len(reader.pages)

    except Exception as error:

        return {
            "valid": False,
            "error_code": "CORRUPTED_FILE",
            "message": "The PDF could not be read successfully.",
            "filename": filename,
            "file_type": "pdf",
            "details": str(error)
        }


    # --------------------------------------------------------
    # Check empty PDF
    # --------------------------------------------------------

    if page_count == 0:

        return {
            "valid": False,
            "error_code": "EMPTY_DOCUMENT",
            "message": "The PDF contains no pages.",
            "filename": filename,
            "file_type": "pdf"
        }


    # --------------------------------------------------------
    # SUCCESS
    # --------------------------------------------------------

    return {
        "valid": True,
        "error_code": None,
        "message": "PDF validation successful.",
        "filename": filename,
        "file_type": "pdf",
        "size_bytes": size_bytes,
        "page_count": page_count
    }


# ============================================================
# CSV VALIDATION
# ============================================================

def validate_csv(
    file_path: Path,
    filename: str,
    size_bytes: int
) -> dict:
    """
    Validate CSV file.

    This performs basic readability validation.
    Final delimiter behavior remains configurable/TBD.
    """

    try:

        # Try UTF-8 first.
        # utf-8-sig also handles files containing a BOM.

        with file_path.open(
            "r",
            encoding="utf-8-sig",
            newline=""
        ) as file:

            # Read a small portion to confirm readability.
            sample = file.read(4096)


    except UnicodeDecodeError as error:

        return {
            "valid": False,
            "error_code": "INVALID_ENCODING",
            "message": "The CSV file is not valid UTF-8 text.",
            "filename": filename,
            "file_type": "csv",
            "details": str(error)
        }


    except Exception as error:

        return {
            "valid": False,
            "error_code": "CORRUPTED_FILE",
            "message": "The CSV file could not be read.",
            "filename": filename,
            "file_type": "csv",
            "details": str(error)
        }


    # Empty content check

    if not sample.strip():

        return {
            "valid": False,
            "error_code": "EMPTY_DOCUMENT",
            "message": "The CSV file contains no data.",
            "filename": filename,
            "file_type": "csv"
        }


    # --------------------------------------------------------
    # SUCCESS
    # --------------------------------------------------------

    return {
        "valid": True,
        "error_code": None,
        "message": "CSV validation successful.",
        "filename": filename,
        "file_type": "csv",
        "size_bytes": size_bytes
    }


# ============================================================
# XLSX VALIDATION
# ============================================================

def validate_xlsx(
    file_path: Path,
    filename: str,
    size_bytes: int
) -> dict:
    """
    Validate XLSX file.

    Checks:
        - Workbook can be opened
        - At least one worksheet exists
    """

    try:

        from openpyxl import load_workbook

        workbook = load_workbook(
            filename=file_path,
            read_only=True,
            data_only=True
        )

        sheet_names = workbook.sheetnames

        sheet_count = len(sheet_names)

        workbook.close()


    except ImportError:

        return {
            "valid": False,
            "error_code": "MISSING_DEPENDENCY",
            "message": (
                "openpyxl is required to validate XLSX files."
            ),
            "filename": filename,
            "file_type": "xlsx"
        }


    except Exception as error:

        return {
            "valid": False,
            "error_code": "CORRUPTED_FILE",
            "message": "The XLSX file could not be opened.",
            "filename": filename,
            "file_type": "xlsx",
            "details": str(error)
        }


    # --------------------------------------------------------
    # Check worksheets
    # --------------------------------------------------------

    if sheet_count == 0:

        return {
            "valid": False,
            "error_code": "EMPTY_DOCUMENT",
            "message": "The XLSX file contains no worksheets.",
            "filename": filename,
            "file_type": "xlsx"
        }


    # --------------------------------------------------------
    # SUCCESS
    # --------------------------------------------------------

    return {
        "valid": True,
        "error_code": None,
        "message": "XLSX validation successful.",
        "filename": filename,
        "file_type": "xlsx",
        "size_bytes": size_bytes,
        "sheet_count": sheet_count,
        "sheet_names": sheet_names
    }


# ============================================================
# JSON VALIDATION
# ============================================================

def validate_json(
    file_path: Path,
    filename: str,
    size_bytes: int
) -> dict:
    """
    Validate JSON file.

    Checks:
        - UTF-8 readability
        - Valid JSON syntax
        - Non-empty JSON value
    """

    try:

        with file_path.open(
            "r",
            encoding="utf-8"
        ) as file:

            data = json.load(file)


    except UnicodeDecodeError as error:

        return {
            "valid": False,
            "error_code": "INVALID_ENCODING",
            "message": "The JSON file is not valid UTF-8.",
            "filename": filename,
            "file_type": "json",
            "details": str(error)
        }


    except json.JSONDecodeError as error:

        return {
            "valid": False,
            "error_code": "INVALID_JSON",
            "message": "The JSON file contains invalid JSON syntax.",
            "filename": filename,
            "file_type": "json",
            "details": str(error)
        }


    except Exception as error:

        return {
            "valid": False,
            "error_code": "CORRUPTED_FILE",
            "message": "The JSON file could not be read.",
            "filename": filename,
            "file_type": "json",
            "details": str(error)
        }


    # --------------------------------------------------------
    # Empty JSON
    # --------------------------------------------------------

    if data is None:

        return {
            "valid": False,
            "error_code": "EMPTY_DOCUMENT",
            "message": "The JSON document is empty.",
            "filename": filename,
            "file_type": "json"
        }


    # --------------------------------------------------------
    # SUCCESS
    # --------------------------------------------------------

    return {
        "valid": True,
        "error_code": None,
        "message": "JSON validation successful.",
        "filename": filename,
        "file_type": "json",
        "size_bytes": size_bytes
    }


# ============================================================
# BACKWARD COMPATIBILITY
# ============================================================

def validate_pdf_file(
    file_path: str,
    filename: str | None = None,
    declared_mime_type: str | None = None,
    max_size_bytes: int | None = None
) -> dict:
    """
    Backward-compatible PDF validation function.
    """

    return validate_file(
        file_path=file_path,
        filename=filename,
        declared_mime_type=declared_mime_type,
        max_size_bytes=max_size_bytes
    )
def validate_pdf(
    file_path: str,
    filename: str | None = None,
    declared_mime_type: str | None = None,
    max_size_bytes: int | None = None
) -> dict:
    """
    Public PDF validation function.

    Kept for compatibility with existing Phase 3 tests.
    """

    return validate_file(
        file_path=file_path,
        filename=filename,
        declared_mime_type=declared_mime_type,
        max_size_bytes=max_size_bytes
    )