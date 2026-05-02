#!/usr/bin/env python3
import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CFG = ROOT / "wayland" / "protocols.json"


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def main() -> None:
    cfg = json.loads(CFG.read_text())
    out_dir = ROOT / cfg["outputDir"]
    out_dir.mkdir(parents=True, exist_ok=True)
    scanner = cfg["scanner"]["binary"]
    ch_mode = cfg["scanner"]["clientHeaderMode"]
    pc_mode = cfg["scanner"]["privateCodeMode"]

    for proto in cfg["protocols"]:
        xml = Path(proto["xml"])
        if not xml.exists():
            print(f"[skip] missing xml: {xml}")
            continue

        ch_out = out_dir / proto["clientHeader"]
        pc_out = out_dir / proto["privateCode"]

        run([scanner, ch_mode, str(xml), str(ch_out)])
        run([scanner, pc_mode, str(xml), str(pc_out)])
        print(f"[ok] {proto['id']} -> {ch_out.name}, {pc_out.name}")


if __name__ == "__main__":
    main()