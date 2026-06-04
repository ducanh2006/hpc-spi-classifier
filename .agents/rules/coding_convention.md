---
trigger: always_on
---

# C++ coding convention for this project

## 1. Naming Conventions
Strictly adhere to DPDK/Linux Kernel C style.
- **Types (Structs, Classes, Unions, Enums):** `snake_case`.
  - Add `_t` suffix for POD structs or those interacting with C APIs (e.g., `struct packet_ctx_t`).
- **Functions & Methods:** `snake_case` (e.g., `init_engine()`, `process_packet()`).
- **Variables & Members:** `snake_case`. No `m_` prefix or `_` suffix.
- **Constants & Macros:** `UPPER_SNAKE_CASE` (for `#define`, `const`, `constexpr`).
- **Enums:** Type name in `snake_case`; enumerators in `UPPER_SNAKE_CASE`.
- **Namespaces:** `snake_case` (if using C++ namespaces).

## 2. Formatting & Layout
Comply with DPDK `checkpatch.pl` standards.
- **Indent:** Use **TABs** only. Set editor **display width to 8**. No spaces for indentation.
- **Line Length:** Less than 100 chars.
- **Braces:**
  - **Functions:** Opening brace on the **next line**.
  - **Control Structures** (`if`, `for`, `while`): Opening brace on the **same line**.
- **Pointers/Refs:** `*` and `&` attach to the **variable name** (e.g., `char *buf`, NOT `char* buf`).
- **Spacing:**
  - Space after keywords (`if`, `for`) and around binary operators (`=`, `+`, `==`).
  - No space between function name and arguments: `func(arg)`.

## 3. Include Header Order
Separate groups with a single blank line. Sort **alphabetically** within each group.
1. **Related Header:** Corresponding `.h` for the current `.c`/`.cpp` file.
2. **C System Headers:** `<stdint.h>`, `<string.h>`, etc.
3. **C++ Standard Library:** `<vector>`, `<memory>` (if applicable).
4. **DPDK Headers:** `<rte_eal.h>`, `<rte_mbuf.h>`, etc.
5. **Hyperscan Headers:** `<hs.h>`, `<hs_common.h>`.
6. **Project Headers:** Other local modules (e.g., `"utils/logger.h"`).

## 4. Header Guards & Compatibility
- **Header Guards:** **MANDATORY** use of `#pragma once` for all `.h` and `.hpp` files. Do NOT use `#ifndef` guards.
- **C/C++ Compatibility:** All public headers exposing C-compatible APIs (structs/functions) **MUST** wrap declarations in `extern "C"` blocks to prevent name mangling.

## 5. Commenting Rules
Follow Linux Kernel/DPDK style. Comments must be in English, concise, and focused on the "WHY". Comment only for complex logic; omit for obvious code.
- **Placement:**
  - **Above the line:** Preferred for logic blocks, complex algorithms, or multi-line explanations.
  - **Inline (Same line):** Allowed only for short variable/constant descriptions. Keep it brief.
- **Syntax:**
  - **Single-line:** Use `//`.
  - **Block:** Use `/* ... */` with aligned asterisks `*` for multi-line detailed explanations.
- **API Documentation:** Use Doxygen style `/** ... */` **ONLY** for public API functions in header files (`.h`). Do not duplicate in `.cpp` implementation files.