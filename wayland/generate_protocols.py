#!/usr/bin/env python3
"""Generate src/Wayland/generated from vendored XML under wayland/xml."""
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CFG = ROOT / "wayland" / "protocols.json"


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def resolve_xml_path(proto: dict) -> Path:
    raw = proto["xml"]
    p = Path(raw)
    if p.is_absolute():
        return p
    return (ROOT / p).resolve()


def cmd_generate() -> None:
    cfg = json.loads(CFG.read_text())
    out_dir = ROOT / cfg["outputDir"]
    out_dir.mkdir(parents=True, exist_ok=True)
    scanner = cfg["scanner"]["binary"]
    ch_mode = cfg["scanner"]["clientHeaderMode"]
    pc_mode = cfg["scanner"]["privateCodeMode"]

    for proto in cfg["protocols"]:
        xml = resolve_xml_path(proto)
        if not xml.exists():
            print(f"[skip] missing xml: {xml}", file=sys.stderr)
            continue

        ch_out = out_dir / proto["clientHeader"]
        pc_out = out_dir / proto["privateCode"]

        run([scanner, ch_mode, str(xml), str(ch_out)])
        run([scanner, pc_mode, str(xml), str(pc_out)])
        print(f"[ok] {proto['id']} -> {ch_out.name}, {pc_out.name}")


def cmd_sync(src: Path | None) -> None:
    cfg = json.loads(CFG.read_text())
    dest = ROOT / cfg.get("xmlRoot", "wayland/xml")
    source = src or Path("/usr/share/wayland-protocols")
    if not source.is_dir():
        print(f"sync: source not found: {source}", file=sys.stderr)
        sys.exit(1)
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, dest, dirs_exist_ok=True)
    print(f"sync: {source} -> {dest}")


def main() -> None:
    if len(sys.argv) >= 2 and sys.argv[1] == "sync":
        extra = Path(sys.argv[2]) if len(sys.argv) >= 3 else None
        cmd_sync(extra)
        return
    cmd_generate()


if __name__ == "__main__":
    main()
