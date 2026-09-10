#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if grep -RniE --include='*.py' --include='*.cpp' --include='*.h' --include='*.yaml' --include='*.yml' --include='*.json' '(ollama|qwen|11434|conversation[._ -]?agent|llm[_ -]?(client|provider))' "$ROOT/Server" "$ROOT/Client"; then
 echo 'FAIL: forbidden LLM runtime reference'; exit 1
fi
echo 'NO-LLM GUARD: PASS'
