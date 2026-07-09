#!/usr/bin/env python3
"""Build the on-card dictionary files the reader's DictionaryStore expects.

Input: a UTF-8 TSV of `word<TAB>definition` lines (duplicate words are merged
with " | "). Get one from WordNet via the `wn` package, wiktextract dumps, or
any glossary you like:

    pip install wn && python -c "
    import wn; wn.download('oewn:2024')
    w = wn.Wordnet('oewn:2024')
    seen = {}
    for word in w.words():
        defs = [s.definition() for s in word.synsets() if s.definition()]
        if defs:
            seen.setdefault(word.lemma().lower(), defs[0])
    with open('words.tsv', 'w') as f:
        for k in sorted(seen):
            f.write(f'{k}\t{seen[k]}\n')
    "

Usage:
    tools/build_dict.py words.tsv out/            # writes out/dict.idx + out/dict.dat
    tools/build_dict.py --demo out/               # tiny sample for smoke tests

Copy the two files to /dict/ on the SD card (USB transfer or companion).

Format (matches src/text/Dictionary.h):
    dict.idx: u32le magic "RDIC", u32le count, then count records of
              24-byte zero-padded lowercase word + u32le offset + u32le length
    dict.dat: definition text blobs (device-renderable ASCII-ish)
"""

import struct
import sys
import unicodedata
from pathlib import Path

MAGIC = 0x43494452  # "RDIC" little-endian
WORD_BYTES = 24
MAX_DEFINITION = 500

DEMO = [
    ("read", "look at and comprehend the meaning of written matter"),
    ("reader", "a person who reads; a device presenting text"),
    ("saccade", "a rapid movement of the eye between fixation points"),
    ("word", "a single distinct meaningful element of speech or writing"),
]


def fold(text: str) -> str:
    """Fold to the device's renderable byte range."""
    text = unicodedata.normalize("NFKD", text)
    out = []
    for ch in text:
        code = ord(ch)
        if 32 <= code < 127 or 0xA0 <= code <= 0xFF:
            out.append(ch)
        elif not unicodedata.combining(ch):
            out.append(" ")
    return " ".join("".join(out).split())


def main() -> int:
    args = sys.argv[1:]
    if len(args) != 2:
        print(__doc__)
        return 2

    out_dir = Path(args[1])
    out_dir.mkdir(parents=True, exist_ok=True)

    entries: dict[str, str] = {}
    if args[0] == "--demo":
        for word, definition in DEMO:
            entries[word] = definition
    else:
        with open(args[0], encoding="utf-8") as f:
            for line in f:
                if "\t" not in line:
                    continue
                word, definition = line.rstrip("\n").split("\t", 1)
                word = fold(word.strip().lower())
                definition = fold(definition.strip())
                if not word or not definition or len(word.encode()) > WORD_BYTES - 1:
                    continue
                if word in entries and definition not in entries[word]:
                    merged = entries[word] + " | " + definition
                    entries[word] = merged[:MAX_DEFINITION]
                else:
                    entries.setdefault(word, definition[:MAX_DEFINITION])

    if not entries:
        print("no usable entries")
        return 1

    words = sorted(entries)
    data = bytearray()
    records = []
    for word in words:
        blob = entries[word].encode("latin-1", errors="replace")
        records.append((word, len(data), len(blob)))
        data += blob

    with open(out_dir / "dict.idx", "wb") as idx:
        idx.write(struct.pack("<II", MAGIC, len(records)))
        for word, offset, length in records:
            idx.write(struct.pack(f"<{WORD_BYTES}sII", word.encode("latin-1"), offset, length))
    (out_dir / "dict.dat").write_bytes(data)

    print(f"{len(records)} words, index {(8 + 32 * len(records)) / 1e6:.1f} MB, "
          f"data {len(data) / 1e6:.1f} MB -> copy to /dict/ on the SD card")
    return 0


if __name__ == "__main__":
    sys.exit(main())
