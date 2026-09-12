"""
Trim main block: keep only the target OP's main, delete other 4 mains + #if/#elif/#else/#endif wrappers.

For each file:
  - Remove the leading `#if defined(HINT_OP_XXX)` line before the kept main
  - Keep the kept main body
  - Delete from `#elif defined(...)` (or `#else` / matching `#endif` of this if-chain) to EOF
  - Drop the trailing `#else\n#error ...\n#endif`

Target OP per file:
  add.cpp -> HINT_OP_ADD
  mul.cpp -> HINT_OP_MUL
  div.cpp -> HINT_OP_DIV
"""
import re
import sys
from pathlib import Path

ROOT = Path(r"d:\precious_speed")
TARGETS = {
    "add.cpp": "HINT_OP_ADD",
    "mul.cpp": "HINT_OP_MUL",
    "div.cpp": "HINT_OP_DIV",
}

# Match `#if defined(HINT_OP_XXX)` at column 0.
IF_RE = re.compile(r"^#if defined\((HINT_OP_\w+)\)\s*\n", re.MULTILINE)
# Match `int main()` at column 0 (allow leading whitespace).
MAIN_RE = re.compile(r"^[ \t]*int main\(\)", re.MULTILINE)
# Match the next `#elif defined(...)` / `#else` at column 0 that closes the kept main.
CLOSE_RE = re.compile(r"^#(?:elif defined\(HINT_OP_\w+\)|else)\s*\n", re.MULTILINE)


def trim_file(path: Path, target_op: str) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")
    # Find the main-block opener: `#if defined(HINT_OP_*)` whose next non-blank line
    # (within 200 chars) is `int main()`. This avoids matching `#if defined(HINT_OP_*)`
    # inside the Integer class body (which guards helper macros / friend operators).
    open_match = None
    for m in IF_RE.finditer(text):
        # Look ahead within 200 chars for `int main()`
        ahead = text[m.end():m.end() + 200]
        if MAIN_RE.search(ahead):
            open_match = m
            break
    if open_match is None:
        print(f"[{path.name}] no `#if defined(HINT_OP_*) ... int main()` found, skip")
        return
    found_op = open_match.group(1)
    if found_op != target_op:
        print(f"[{path.name}] WARN: main block op is {found_op}, expected {target_op}")
        target_op = found_op

    start = open_match.end()  # right after `#if defined(...)\n`
    # Find the matching close (`#elif` or `#else`) at col 0
    close_match = CLOSE_RE.search(text, start)
    if not close_match:
        print(f"[{path.name}] no #elif/#else after main open, skip")
        return
    end = close_match.start()  # up to but not including #elif/#else

    kept_main = text[start:end]
    # Build new file: prefix (everything before #if) + kept_main
    new_text = text[: open_match.start()] + kept_main
    # Ensure file ends with a newline
    if not new_text.endswith("\n"):
        new_text += "\n"

    old_lines = text.count("\n")
    new_lines = new_text.count("\n")
    print(f"[{path.name}] kept main={target_op}, lines {old_lines} -> {new_lines} (-{old_lines - new_lines})")
    path.write_text(new_text, encoding="utf-8", newline="\n")


def main() -> int:
    only = sys.argv[1] if len(sys.argv) > 1 else None
    for name, op in TARGETS.items():
        if only and only != name:
            continue
        trim_file(ROOT / name, op)
    return 0


if __name__ == "__main__":
    sys.exit(main())
