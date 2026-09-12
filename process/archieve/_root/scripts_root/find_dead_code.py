"""
Find dead code in add/mul/div.cpp by analyzing function call graph.

Strategy:
1. Extract all static method/function definitions with their names
2. Find all call sites (function name references)
3. A function is "live" if it's called from main or from another live function
4. A function is "dead" if not reachable from main

Note: This is a conservative analysis. Template instantiations and
macro-generated code may not be caught. Manual review required.
"""
import re
import sys
from pathlib import Path

ROOT = Path(r"d:\precious_speed")

# Match function definitions: static <type> <name>(...) at column 0 (inside class)
# Also match friend operators
FUNC_DEF_RE = re.compile(
    r"^\s*(?:static\s+|inline\s+|friend\s+)*"
    r"(?:[\w:<>*&]+\s+)+"  # return type
    r"(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:\{|\{|$)",
    re.MULTILINE,
)


def extract_functions(text):
    """Extract function names defined in the file."""
    funcs = set()
    for m in re.finditer(r"^\s*(?:static\s+|inline\s+)*([\w:<>*&]+\s+)+(\w+)\s*\([^)]*\)", text, re.MULTILINE):
        name = m.group(2)
        if name not in ("if", "for", "while", "switch", "return", "sizeof", "typedef", "using"):
            funcs.add(name)
    return funcs


def find_call_sites(text):
    """Find all function name references (call sites)."""
    # Match word boundaries
    words = set(re.findall(r"\b(\w+)\b", text))
    return words


def main():
    for name in ["add.cpp", "mul.cpp", "div.cpp"]:
        path = ROOT / name
        text = path.read_text(encoding="utf-8", errors="replace")

        # Extract function definitions (simple heuristic)
        funcs = {}
        for m in re.finditer(
            r"^\s*(?:static\s+|inline\s+|friend\s+)*(?:constexpr\s+)?(?:[\w:<>*&]+\s+)+(\w+)\s*\([^)]*\)\s*(?:const)?(?:\s*\{|\s*$)",
            text, re.MULTILINE,
        ):
            fname = m.group(1)
            if fname in ("if", "for", "while", "switch", "return", "sizeof", "typedef", "using", "class", "struct", "namespace", "operator"):
                continue
            funcs[fname] = m.start()

        # Find all words used in the file (call sites + references)
        used_words = find_call_sites(text)

        # main is always live
        live = {"main"}
        # Anything referenced in main's body is live
        # Also, standard library and std:: stuff is not our function

        # Simple reachability: a function is live if its name appears in the file
        # (either definition or call). Since we only care about dead code
        # (functions defined but never called), we check if the name appears
        # more than once (definition + at least one call).
        dead = []
        for fname, pos in funcs.items():
            # Count occurrences of the function name (as whole word)
            count = len(re.findall(r"\b" + re.escape(fname) + r"\b", text))
            # If count == 1, only the definition exists, no calls
            if count <= 1 and fname not in ("main",):
                line = text[:pos].count("\n") + 1
                dead.append((line, fname))

        dead.sort()
        print(f"\n=== {name}: {len(dead)} potentially dead functions ===")
        for line, fname in dead[:40]:
            print(f"  L{line}: {fname}")
        if len(dead) > 40:
            print(f"  ... and {len(dead) - 40} more")


if __name__ == "__main__":
    main()
