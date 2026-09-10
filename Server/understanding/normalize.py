import re, unicodedata

def normalize(text: str) -> str:
    text = unicodedata.normalize("NFC", text or "").strip().lower()
    text = re.sub(r"[!?.,;:]+", " ", text)
    text = re.sub(r"\s+", " ", text)
    return text.strip()
