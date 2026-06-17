#!/usr/bin/env python3
"""Generate tz.json (embedded in firmware) from scripts/tz.ts.

The web admin page serves this JSON so the browser can render a timezone
dropdown of friendly names while POSTing the POSIX TZ string to the device.
Edit scripts/tz.ts to update the timezone list, then re-run this script.

Usage:
    python3 scripts/gen_tz.py
"""

import json
import re
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "scripts" / "tz.ts"
OUT = ROOT / "bitclock-fw" / "main" / "web" / "tz.json"

# Match lines like: ["America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"],
ENTRY_RE = re.compile(r'\[\s*"([^"]+)"\s*,\s*"([^"]*)"\s*\]')


def main():
    text = SRC.read_text()
    entries = []
    for label, posix in ENTRY_RE.findall(text):
        # A stray "]" snuck into one upstream value; strip trailing brackets.
        posix = posix.rstrip("]")
        entries.append([label, posix])

    if not entries:
        raise SystemExit(f"No timezone entries parsed from {SRC}")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    # Compact JSON to minimize embedded firmware size.
    OUT.write_text(json.dumps(entries, separators=(",", ":")))
    print(f"Wrote {len(entries)} timezones to {OUT}")


if __name__ == "__main__":
    main()
