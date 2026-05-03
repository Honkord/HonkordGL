#!/usr/bin/env python3
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = ROOT / "bindings" / "spec.json"
KOTLIN_OUT_DIR = ROOT / "honkord-jvm" / "src" / "main" / "kotlin" / "org" / "honkord" / "generated"
NATIVE_OUT = ROOT / "native-bridge" / "src" / "generated_bindings.cpp"


def emit_kotlin(spec: dict) -> None:
    KOTLIN_OUT_DIR.mkdir(parents=True, exist_ok=True)
    package = spec["namespace"]
    for cls in spec["classes"]:
        lines = [f"package {package}", "", f"object {cls['name']} {{", "    init {", '        System.loadLibrary("honkord_jni_bridge")', "    }", ""]
        for m in cls["methods"]:
            kotlin_ret = "Int" if m["return"] == "int" else "Unit"
            lines.append(f"    external fun {m['name']}(): {kotlin_ret}")
        lines.append("}")
        (KOTLIN_OUT_DIR / f"{cls['name']}.kt").write_text("\n".join(lines) + "\n")


def emit_native(spec: dict) -> None:
    lines = [
        '#include <jni.h>',
        '#include <HonkordGL/WindowBackend.h>',
        '#include <HonkordGL/Video.h>',
        '#include <HonkordGL/Sprite.h>',
        "",
        "// Auto-generated JNI stubs. Replace bodies with full API wiring as bindings expand.",
        ""
    ]
    for cls in spec["classes"]:
        for m in cls["methods"]:
            jni_name = f"Java_org_honkord_generated_{cls['name']}_{m['name']}"
            lines.append(f'extern "C" JNIEXPORT jint JNICALL {jni_name}(JNIEnv*, jclass)')
            lines.append("{")
            lines.append("    return 1;")
            lines.append("}")
            lines.append("")
    NATIVE_OUT.write_text("\n".join(lines))


def main() -> None:
    spec = json.loads(SPEC.read_text())
    emit_kotlin(spec)
    emit_native(spec)
    print("Generated bindings from", SPEC)


if __name__ == "__main__":
    main()
