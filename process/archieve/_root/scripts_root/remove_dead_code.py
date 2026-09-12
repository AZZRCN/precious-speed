"""Remove dead code functions from div.cpp/add.cpp/mul.cpp.
Dead functions are defined but never called (only 1 occurrence in file).
Preserves: deallocate (allocator required), _prof_flush (profiling), absDivNewtonWithInvLoose (already #if 0).
"""
import re
import sys

DEAD_FUNCS = ['int_floor2', 'fill_zero', 'expandLog', 'isEven', 'lengthBase10', 'from_c_str', 'to_c_str']

def remove_func(text, func_name):
    """Find function definition by name, match brace depth, remove it."""
    pattern = re.compile(
        r'([ \t]*(?:template\s*<[^>]*>\s*\n)?'
        r'[ \t]*(?:static\s+|inline\s+|constexpr\s+)*'
        r'(?:[\w:<>*&]+\s+)+'
        + re.escape(func_name) + r'\s*\([^)]*\)\s*(?:const)?\s*\{)',
        re.MULTILINE
    )
    m = pattern.search(text)
    if not m:
        return text, False
    start = m.start()
    brace_pos = m.end() - 1  # position of '{'
    depth = 1
    i = brace_pos + 1
    in_str = False
    in_chr = False
    in_lc = False
    in_bc = False
    while i < len(text) and depth > 0:
        c = text[i]
        if in_lc:
            if c == '\n':
                in_lc = False
        elif in_bc:
            if c == '*' and i + 1 < len(text) and text[i + 1] == '/':
                in_bc = False
                i += 1
        elif in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif in_chr:
            if c == '\\':
                i += 1
            elif c == "'":
                in_chr = False
        else:
            if c == '/' and i + 1 < len(text):
                if text[i + 1] == '/':
                    in_lc = True
                    i += 1
                elif text[i + 1] == '*':
                    in_bc = True
                    i += 1
            elif c == '"':
                in_str = True
            elif c == "'":
                in_chr = True
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
        i += 1
    end = i  # includes closing '}'
    # Remove preceding whitespace/empty lines
    while start > 0 and text[start - 1] in ' \t\n':
        start -= 1
    if start > 0 and text[start] == '\n':
        start += 1
    return text[:start] + text[end:], True


def main():
    dry_run = '--dry' in sys.argv
    files = [r'd:\precious_speed\div.cpp', r'd:\precious_speed\add.cpp', r'd:\precious_speed\mul.cpp']
    for fp in files:
        text = open(fp, encoding='utf-8', errors='replace').read()
        orig_len = len(text)
        print(f"=== {fp} ===")
        for func in DEAD_FUNCS:
            text, ok = remove_func(text, func)
            print(f"  {func}: {'removed' if ok else 'NOT FOUND'}")
        if not dry_run:
            open(fp, 'w', encoding='utf-8').write(text)
        print(f"  size: {orig_len} -> {len(text)} bytes ({len(text) - orig_len:+d})")
        print()


if __name__ == '__main__':
    main()
