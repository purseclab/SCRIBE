# scripts/

Standalone tools used while preparing or experimenting with cases.

| Script | Purpose |
| --- | --- |
| `decompile_func.py` | Run IDA on a binary and emit `decompiled.c`, `symbols.json`, `localvars.json`, headers — the same `from_decompiler/` shape the cases consume. |
| `dump_asm_and_decomp.py` | Dump assembly and decompiled output for a single function (debugging aid). |
| `llm_patch.py` | LLM-driven patch application (SEARCH/REPLACE blocks) for a CVE case. Reads `OPENAI_API_KEY` (and optional `OPENAI_BASE_URL`) from the environment. |

These are tools, not a pipeline. The cases under `cases/` run themselves
via their own `c_patch.py` and `test.sh`; nothing here is required for a
case to work.
