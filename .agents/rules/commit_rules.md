---
trigger: always_on
---

#  Git Commit Rules
Strictly adhere to the **Conventional Commits** standard to ensure a clear, readable project history and enable automated CHANGELOG generation.

## Mandatory Structure
```text
<type>(<scope>): <subject>

[optional body]

[optional footer(s)]
```

## Type (Change Type) - Mandatory
Use only the following types:
*   `feat`: A new feature.
*   `fix`: A bug fix.
*   `perf`: A performance improvement. **(Critical for DPDK projects)**
*   `refactor`: A code change that neither fixes a bug nor adds a feature.
*   `docs`: Documentation only changes.
*   `style`: Changes that do not affect the meaning of the code (white-space, formatting, missing semi-colons, etc.).
*   `test`: Adding missing tests or correcting existing tests.
*   `build`: Changes that affect the build system or external dependencies (Makefile, CMake, dpdk-meson).
*   `ci`: Changes to our CI configuration files and scripts.
*   `chore`: Other changes that don't modify src or test files.

## Scope (Optional but Recommended)
Specify the module or component affected.
*   Examples: `dpi`, `mempool`, `hyperscan`, `worker`, `cli`.
*   If the change affects the entire project, the scope may be omitted.

## Subject (Title) - Mandatory
*   Write in **English**.
*   Use the imperative mood in the present tense: "add" instead of "added" or "adds".
*   **Do not** capitalize the first letter.
*   **Do not** end with a period (`.`).
*   Limit length to **50-72 characters**.

## Body (Optional)
*   Explain the **WHY** and **HOW** of the change.
*   Separate from the subject line by a blank line.
*   Use bullet points for key details if necessary.

## Footer (Optional)
*   Reference issues or tickets: `Closes #123`, `Fixes #456`.
*   Note breaking changes: `BREAKING CHANGE: <description>`.

## Examples

✅ **Good:**
```text
perf(dpi): optimize hyperscan scratch allocation

- Reuse scratch space per worker thread to avoid malloc overhead.
- Align scratch memory to 64-byte cache line for better performance.

Closes #42
```

✅ **Good (Simple Fix):**
```text
fix(mempool): correct buffer overflow in packet burst
```

❌ **Bad:**
```text
fix mempool bug
```
*(Reason: Incorrect format, lacks type, vague subject)*

❌ **Bad:**
```text
feat: added new feature for logging.
```
*(Reason: Capitalized first letter, ends with period, uses past tense "added")*