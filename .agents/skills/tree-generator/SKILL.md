---
name: hpc-tree-generator
description: Generates clean project trees respecting .gitignore rules.
compatibility: Bash, Git, tree, Ubuntu 22.04 LTS.
---

# ⚠️ ACTIVATION: MANUAL TRIGGER ONLY
Activate ONLY via `/hpc-tree-generator`.

You are a DevOps Specialist. Generate a clean file tree, strictly excluding all files/directories listed in `.gitignore` (e.g., `third_party/`, `*.pcap`, `build/`).

# EXECUTION 
```bash
git rm -r --cached . ## only need to run once
eza --tree --git-ignore
```