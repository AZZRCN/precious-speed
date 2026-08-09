"""
Fix AlignedAlloc32 for MinGW (posix_memalign -> _aligned_malloc) and trim main block.

Step 1: Replace posix_memalign-based allocate/deallocate with #ifdef _WIN32 conditional.
Step 2: Trim main block (keep only the target OP's main, delete other 4 mains + #if/#endif).
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

# --- Step 1: Fix AlignedAlloc32 ---
# Match: `if (posix_memalign(&p, 32, n * sizeof(T)) != 0)\n                throw std::bad_alloc();`
ALLOC_RE = re.compile(
    r"if \(posix_memalign\(&p, 32, n \* sizeof\(T\)\) != 0\)\s*\n(\s*)throw std::bad_alloc\(\);"
)
# Match: `void deallocate(T *p, size_t) { free(p); }`
DEALLOC_RE = re.compile(r"void deallocate\(T \*p, size_t\) \{ free\(p\); \}")

ALLOC_REPLACEMENT = (
    "#ifdef _WIN32\n"
    "{ind}p = _aligned_malloc(n * sizeof(T), 32);\n"
    "{ind}if (!p) throw std::bad_alloc();\n"
    "#else\n"
    "{ind}if (posix_memalign(&p, 32, n * sizeof(T)) != 0)\n"
    "{ind}    throw std::bad_alloc();\n"
    "#endif"
)
DEALLOC_REPLACEMENT = (
    "void deallocate(T *p, size_t)\n"
    "        {\n"
    "#ifdef _WIN32\n"
    "            _aligned_free(p);\n"
    "#else\n"
    "            free(p);\n"
    "#endif\n"
    "        }"
)


def fix_memalign(text: str) -> str:
    # Idempotent: if already patched (_aligned_malloc present), skip.
    if "_aligned_malloc" in text:
        return text
    def alloc_sub(m):
        ind = m.group(1)
        return ALLOC_REPLACEMENT.format(ind=ind)
    text = ALLOC_RE.sub(alloc_sub, text)
    text = DEALLOC_RE.sub(DEALLOC_REPLACEMENT, text)
    return text


# --- Step 2: Trim main block ---
# Main block structure:
#   #if defined(HINT_OP_ADD)      <- block open (first #if with HINT_OP_*)
#   int main() { ... }
#   #elif defined(HINT_OP_MUL)
#   int main() { ... }
#   #elif defined(HINT_OP_DIV)
#   int main() { ... }
#   #elif defined(HINT_OP_TESTMOD)
#   int main() { ... }
#   #elif defined(HINT_OP_TESTNEWTON)
#   int main() { ... }
#   #else
#   #error "..."
#   #endif                         <- block close
#
# For target_op = HINT_OP_ADD: keep the #if branch's main
# For target_op = HINT_OP_MUL: keep the #elif HINT_OP_MUL branch's main
# etc.

# Find the first `#if defined(HINT_OP_*)` whose next non-blank line is `int main()`.
BLOCK_OPEN_RE = re.compile(r"^#if defined\((HINT_OP_\w+)\)\s*\n", re.MULTILINE)
MAIN_RE = re.compile(r"^[ \t]*int main\(\)", re.MULTILINE)
# Find `#if ` / `#elif ` / `#else` / `#endif` at column 0.
# IMPORTANT: must NOT match `#ifdef` / `#ifndef` (they start with `#if` too).
# Use `if[ \t(]` to require a space/tab/paren after `if`, excluding `ifdef`/`ifndef`.
BRANCH_RE = re.compile(r"^#(if[ \t(]|elif[ \t(]|else\b|endif\b)[^\n]*\n", re.MULTILINE)


def trim_main(text: str, path_name: str, target_op: str) -> str:
    # 1. Find main-block open: first `#if defined(HINT_OP_*)` followed by `int main()`.
    block_open = None
    for m in BLOCK_OPEN_RE.finditer(text):
        ahead = text[m.end():m.end() + 200]
        if MAIN_RE.search(ahead):
            block_open = m
            break
    if block_open is None:
        print(f"[{path_name}] no main block found, skip trim")
        return text

    # 2. Walk branches to find block end and the target_op branch's main body.
    #    Branches: #if, #elif, #else, #endif (all at column 0).
    pos = block_open.end()
    block_end = None
    current_op = block_open.group(1)  # the op of the current branch
    # If the first branch (#if) is the target, its main starts right after #if line.
    target_main_start = block_open.end() if current_op == target_op else None
    target_main_end = None
    depth = 1  # we're inside the #if

    while depth > 0:
        m = BRANCH_RE.search(text, pos)
        if m is None:
            print(f"[{path_name}] unterminated #if, skip trim")
            return text
        directive = m.group(1)
        line = m.group(0)
        # Update current_op / track target branch
        if directive.startswith("if"):
            depth += 1
        elif directive.startswith("endif"):
            depth -= 1
            if depth == 0:
                # This is the block_end
                block_end = m.start()
                # Close the previous branch's main body
                if current_op == target_op and target_main_start is not None:
                    target_main_end = m.start()
                break
        elif depth == 1:
            # Only process #elif/#else at the main-block level (depth == 1).
            # Nested #elif/#else (depth > 1) belong to inner #if and must be skipped.
            if directive.startswith("elif"):
                # Close the previous branch's main body
                if current_op == target_op and target_main_start is not None:
                    target_main_end = m.start()
                # Parse the new op from `#elif defined(HINT_OP_XXX)`
                elif_m = re.match(r"#elif defined\((HINT_OP_\w+)\)", line)
                if elif_m:
                    current_op = elif_m.group(1)
                    if current_op == target_op:
                        target_main_start = m.end()
                else:
                    current_op = None  # #elif without HINT_OP_* (shouldn't happen)
            elif directive.startswith("else"):
                # Close the previous branch's main body
                if current_op == target_op and target_main_start is not None:
                    target_main_end = m.start()
                current_op = None  # #else branch, no HINT_OP_*
        pos = m.end()

    if block_end is None:
        print(f"[{path_name}] no block_end found, skip trim")
        return text
    if target_main_start is None or target_main_end is None:
        print(f"[{path_name}] target_op {target_op} branch not found, skip trim")
        return text

    kept_main = text[target_main_start:target_main_end]
    new_text = text[: block_open.start()] + kept_main + text[block_end:]
    if not new_text.endswith("\n"):
        new_text += "\n"
    return new_text


def process(path: Path, target_op: str) -> None:
    text = path.read_text(encoding="utf-8", errors="replace")
    old_lines = text.count("\n")
    text = fix_memalign(text)
    text = trim_main(text, path.name, target_op)
    new_lines = text.count("\n")
    print(f"[{path.name}] lines {old_lines} -> {new_lines} (-{old_lines - new_lines})")
    path.write_text(text, encoding="utf-8", newline="\n")


def main() -> int:
    only = sys.argv[1] if len(sys.argv) > 1 else None
    for name, op in TARGETS.items():
        if only and only != name:
            continue
        process(ROOT / name, op)
    return 0


if __name__ == "__main__":
    sys.exit(main())
