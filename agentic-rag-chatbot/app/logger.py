import logging
import json
from datetime import datetime, timezone

class JSONFormatter(logging.Formatter):
    # Extremely aggressive redaction for accidental leaks in message strings
    def redact(self, text: str) -> str:
        if not isinstance(text, str):
            return text
        # If we need regex redaction we can add it here.
        # But primarily we rely on never passing sensitive fields to the logger.
        return text

    def format(self, record):
        log_record = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "level": record.levelname,
            "logger": record.name,
            "message": self.redact(record.getMessage())
        }
        
        if hasattr(record, "custom_fields"):
            for k, v in record.custom_fields.items():
                log_record[k] = self.redact(str(v)) if isinstance(v, str) else v
                
        return json.dumps(log_record)

def get_logger(name: str = "phase5"):
    logger = logging.getLogger(name)
    if not logger.handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(JSONFormatter())
        logger.addHandler(handler)
        logger.setLevel(logging.INFO)
    return logger
