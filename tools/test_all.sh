#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
"$ROOT/tools/check_no_llm.sh"
PYTHONPATH="$ROOT/Server" python "$ROOT/Server/tests/test_server.py"
g++ -std=c++17 -I"$ROOT/Client" "$ROOT/Client/tests/test_client.cpp" -o /tmp/vipi_v2_client_tests
/tmp/vipi_v2_client_tests
echo 'ALL HOST TESTS: PASS'
