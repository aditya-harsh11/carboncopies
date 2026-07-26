# CarbonGPT (LangChain.js port)

Hand-ported LangChain.js equivalent of `CarbonGPT-main-Chatflow.json`, the Flowise export at the repo root. Runs on [Bun](https://bun.sh).

## Why hand-ported instead of using the repo's `flowise-to-lc` CLI

This chatflow uses several Flowise node types the converter's registry doesn't support
(`toolAgent`, `retrieverTool`, `localAIEmbeddings`, `chatOpenAICustom`, `inputModerationSimple`,
`SQLiteRecordManager`), and its agent converter resolves upstream variable names with a
hardcoded fallback (`llm`, `tools`, `memory`) rather than following the actual graph edges
(see `src/registry/converters/agent.ts`). Running the CLI on this file produces
incomplete/non-functional output, so this directory is a direct, faithful translation instead.

## What was ported

Only the nodes actually wired into `toolAgent_0` (the live execution graph) are included:

| Flowise node | Role | Where |
| --- | --- | --- |
| `inputModerationSimple_0` | Deny-list input moderation | `src/moderation.ts` |
| `chatOpenAICustom_0` | Tool-calling model (vLLM, OpenAI-compatible) | `src/agent.ts` |
| `bufferMemory_1` | Chat history | in-memory `Map` in `src/agent.ts` |
| `localAIEmbeddings_0` + `chroma_0` + `retrieverTool_0` | `braingenix-documents` tool | `src/retrieverTools.ts` |
| `localAIEmbeddings_3` + `chroma_2` + `retrieverTool_2` | `research-documents` tool | `src/retrieverTools.ts` |
| `toolAgent_0` system message | CarbonGPT persona/instructions | `src/systemPrompt.ts` |

### Not ported (present in the export but disconnected from `toolAgent_0`)

- `chroma_1` / `localAIEmbeddings_2` / `SQLiteRecordManager_1` / `retrieverTool_1` — a second
  "ccfwebsite-documents" (Carboncopies collection) retriever tool that exists in the canvas but
  has no edge into the agent's `tools` input.
- `chatAnthropic_0`, `chatOpenAI_0`, `chatOllama_0`, `chatGoogleGenerativeAI_0`, `chatMistralAI_0`,
  `chatPromptTemplate_0`, `calculator_0` — alternate/unused models and tools, also with no edges
  into the agent.
- `SQLiteRecordManager_0` — only used for document-ingestion dedup, not for querying at runtime
  (both Chroma nodes have an empty `document` input, i.e. this flow only queries an
  already-populated collection).

If you want the CCF-website tool included too, add a third `buildRetrieverTool(...)` call in
`src/retrieverTools.ts` with its collection name/description and wire it into the `tools` array.

## Setup

```bash
cd carbongpt-langchain
bun install
cp .env.example .env   # edit if your endpoints differ from the flow's defaults
```

You need, reachable from wherever you run this:
- The vLLM (or any OpenAI-compatible) endpoint serving the tool-calling model
- A LocalAI-compatible embeddings server
- A Chroma server with the `BrainGenix` and `ResearchDocuments` collections already populated

## Run

```bash
bun run start "What is whole brain emulation?"
```

Bun loads `.env` automatically. Each CLI invocation is a fresh process, so conversation memory
only persists within a single run — see `askCarbonGPT(input, sessionId)` in `src/agent.ts` if you
want to wire this into a long-lived server and keep `sessionId` per user/chat.

## Known gaps vs. the original Flowise flow

- **Source document citations**: Flowise's retriever tool has `returnSourceDocuments: true`,
  surfacing snippet citations in its UI. `createRetrieverTool` here returns concatenated page
  content only; wire up `responseFormat: "content_and_artifact"` and handle the tool's artifact
  output if you need citations back out.
- **Detailed streaming**: `toolAgent_0.enableDetailedStreaming` (intermediate step streaming) isn't
  wired up; `AgentExecutor.invoke` returns the final answer only.
