# CarbonGPT - an internal conversational/semantic search tool for Carboncopies Foundation platform development 
### Use cases being explored:
- learning or relearning API calls and how parameter tuning works for platform development
- Whole Brain Emulation 101: How to use chatbots as a check for understanding. What are the best ways to learn using LLM's?
- Benchmark questions acting as unit tests.
- Philosophical texts on consciousness.
### To get started, just head to [chat.carboncopies.org](https://chat.carboncopies.org) during platform development and ask relevant questions.
---
## CarbonGPT Development notes:
![](https://i.imgur.com/0rA8EjZ.png)

## How it works
This repo is the ingestion/backend half of CarbonGPT: it takes source documents (research PDFs/DOCX, the Carboncopies website content, the BrainGenix codebases, etc.), cleans and chunks them, embeds each chunk via a local llama.cpp embedding server, and upserts the results into a persistent [ChromaDB](https://www.trychroma.com/) vector store. That vector store is what the Flowise-based chat frontend queries at query time.

Pipeline, end to end:
1. `gdrive_downloader.py` pulls raw source files from a shared Google Drive folder into `Dataset/ResearchDocuments/downloaded_drive_files/`.
2. `convert_to_llm_format.py` converts those raw files (PDF/DOCX/PPTX/XLSX/ODS) into plain markdown under `Dataset/ResearchDocuments/llm_ready_markdown/`.
3. `Upsert.py` walks `Dataset/` (one subdirectory = one Chroma collection), cleans/chunks each file, embeds the chunks against a running llama.cpp server, and upserts them into the `chroma/` persistent database.
4. `ViewDB.py` is a quick sanity-check script to inspect what's actually in the database.

## Repo layout

### Top-level scripts
- **`Upsert.py`** — The core ingestion engine. Contains the `LlamaCppEmbeddingFunction` (calls a local llama.cpp OpenAI-compatible `/v1/embeddings` endpoint), `clean_content()` (strips license headers, tool-call artifacts, HTML, and other boilerplate before embedding), `chunk_code_file()` / `chunk_text_file()` (semantic chunking for code files by function/class boundaries, falling back to fixed-size 350-char/50-char-overlap chunking for everything else), and `process_directory_to_collection()` (walks a directory, chunks + embeds + upserts in batches of 10 with retry/backoff, one Chroma collection per top-level `Dataset/` subdirectory). Run directly via `python3 Upsert.py [--dataset ./Dataset] [--db ./chroma] [--model embedding-model] [--api-url http://localhost:8082]` — this re-walks and re-upserts every collection.
- **`upsert_research.py`** — A narrow wrapper around `Upsert.py`'s `process_directory_to_collection()` that only (re)upserts the `Dataset/ResearchDocuments` folder into the `ResearchDocuments` collection, without touching/wiping the other collections. Useful when you've only refreshed research docs and don't want a full re-index.
- **`gdrive_downloader.py`** — Standalone script to mirror a public Google Drive folder locally. Reads `GOOGLE_API_KEY` and `DRIVE_FOLDER_ID` from `.env`, lists files in the folder, resolves shortcuts, exports native Google Docs/Sheets/Slides to PDF/XLSX/PPTX (with DOCX/web-export fallbacks for oversized files), and downloads everything into `Dataset/ResearchDocuments/downloaded_drive_files/`.
- **`convert_to_llm_format.py`** — Converts everything in `Dataset/ResearchDocuments/downloaded_drive_files/` into LLM-friendly markdown using [`markitdown`](https://github.com/microsoft/markitdown) (with a `python-docx`-based fallback for `.docx`), writing output to `Dataset/ResearchDocuments/llm_ready_markdown/`.
- **`ViewDB.py`** — Debug/inspection utility. Connects to the `chroma/` persistent client, lists every collection with its document count, and prints 3 randomly sampled documents (content + metadata) per collection so you can eyeball what got indexed.
- **`requirements.txt`** — Python dependencies (`chromadb`, `markitdown[all]`, `python-docx`).
- **`.env`** — Local secrets, not committed: `GOOGLE_API_KEY` and `DRIVE_FOLDER_ID` used by `gdrive_downloader.py`.
- **`upsert.log`** — Captured stdout/stderr from a past `Upsert.py` run; useful as a reference for what a full ingestion run looks like, not meant to be maintained by hand.

### Directories
- **`Dataset/`** — The corpus that gets embedded. Each immediate subdirectory becomes its own Chroma collection when `Upsert.py` runs:
  - `Dataset/ResearchDocuments/` — `downloaded_drive_files/` (raw PDFs/DOCX pulled from Drive) and `llm_ready_markdown/` (their converted markdown counterparts, produced by `convert_to_llm_format.py`).
  - `Dataset/Carboncopies/` — Carboncopies Foundation website source (`CCF-Website/`, an mkdocs-based site) plus standalone notes like `img-processing-library.md`.
  - `Dataset/BrainGenix/` — Local checkouts of the BrainGenix codebases (`BrainGenix-NES`, `BrainGenix-API`, `BrainGenix-VSDA`) plus `BrainGenixInstall.txt`, indexed so the chatbot can answer questions about BrainGenix source/APIs.
- **`chroma/`** — The persistent ChromaDB store itself (SQLite metadata file `chroma.sqlite3` plus one directory of HNSW index files per collection). Generated/managed by `chromadb`, not hand-edited.
- **`py/`** — A local Python virtualenv (`source py/bin/activate`) used to run the ingestion scripts with the right dependencies installed.
- **`BrainGenix/`, `ResearchDocuments/`** — Older, top-level copies of the same source trees now organized under `Dataset/BrainGenix` and `Dataset/ResearchDocuments`. Left over from before the `Dataset/` layout was introduced; `Upsert.py` only reads from `Dataset/`, so these are effectively stale/legacy.

Note: `.gitignore` excludes `Dataset/`, `chroma/`, and `py/` (they're large/generated), but earlier commits added files under those paths before the ignore rule existed, so `git status` may still show changes inside them (e.g. `chroma/chroma.sqlite3`, files under `Dataset/ResearchDocuments/downloaded_drive_files/`).

### How to run backend:
#### Upsert.py
```bash
source py/bin/activate
```
```bash
python3 Upsert.py
```
#### ChromaDB
```bash
sudo systemctl restart Chroma
``` 
```bash
sudo journalctl -u Chroma.service -n 50 --no-pager
```


### How to run frontend:
#### Uses [Flowise](https://docs.flowiseai.com/). A node based LLM prototyping tool. Useful for tool call LLM's (MCP). [Simply a fork of their main branch and customize to your needs](https://github.com/FlowiseAI/Flowise)
- Many permission issues or ssh issues. Contact richard, thomas if need
- future proj: move out of sqlite if too slow
- Resource for vector db understanding https://realpython.com/chromadb-vector-database/

linear sample issue
