import csv
import json
import requests
import sys
from datetime import datetime, timezone

import anthropic

# Configuration
CARBONGPT_API_URL = "http://pve.braingenix.org:3000/api/v1/prediction/234ce643-281e-45ed-b90c-6ba1bbcc5577"
HEADERS = {"Content-Type": "application/json"}
INPUT_CSV = "LLM Question Answer - Sheet1.csv"
OUTPUT_CSV = "benchmark_results.csv"
SUMMARY_FILE = "benchmark_summary.txt"
JUDGE_MODEL = "claude-sonnet-5"
MAX_CHUNKS_FOR_JUDGE = 20
MAX_CHUNK_CHARS = 800

# Reads ANTHROPIC_API_KEY from the environment.
judge_client = anthropic.Anthropic()

JUDGE_SCHEMA = {
    "type": "object",
    "properties": {
        "score": {"type": "integer", "description": "1-5 accuracy/completeness rating vs the Expected Answer"},
        "faithful": {"type": "boolean", "description": "True if every claim in the Actual Answer is supported by the retrieved chunks"},
        "context_recall": {"type": "boolean", "description": "True if the info needed to state the Expected Answer is present somewhere in the retrieved chunks, regardless of whether the Actual Answer used it"},
    },
    "required": ["score", "faithful", "context_recall"],
    "additionalProperties": False,
}


def ask_carbongpt(question):
    """Returns (answer_text, retrieved_chunks) for the given question."""
    payload = {"question": question, "overrideConfig": {"returnSourceDocuments": True}}
    try:
        response = requests.post(CARBONGPT_API_URL, headers=HEADERS, json=payload, timeout=120)
        response.raise_for_status()
        data = response.json()
        answer = data.get('text', data.get('answer', data.get('content', str(data))))
        chunks = [doc.get('pageContent', '') for doc in data.get('sourceDocuments', [])]
        return answer, chunks
    except Exception as e:
        return f"ERROR: {e}", []


def rate_answer(question, expected, actual, chunks):
    """Returns a dict: {score, faithful, context_recall}. faithful is None if no
    chunks were returned (nothing to check the answer's groundedness against).
    context_recall is False (not None) when no chunks were returned -- recall
    over an empty retrieved set is always a fail, that's not an unknown."""
    if not chunks:
        # Nothing retrieved: still ask the judge to score the answer against
        # the expected answer, but faithful/context_recall are deterministic.
        prompt = f"""You are an expert evaluator grading an AI assistant's answer to a question.

Question: {question}
Expected Answer: {expected}
Actual Answer: {actual}

No context was retrieved for this question.

Score the Actual Answer 1-5 on accuracy and completeness compared to the Expected Answer
(1 = completely wrong/irrelevant, 5 = perfectly accurate and complete)."""
        try:
            response = judge_client.messages.create(
                model=JUDGE_MODEL,
                max_tokens=100,
                thinking={"type": "disabled"},
                output_config={
                    "effort": "low",
                    "format": {"type": "json_schema", "schema": {
                        "type": "object",
                        "properties": {"score": {"type": "integer"}},
                        "required": ["score"],
                        "additionalProperties": False,
                    }},
                },
                messages=[{"role": "user", "content": prompt}],
            )
            if response.stop_reason == "refusal":
                return {"score": "Judge Error: refused", "faithful": None, "context_recall": False}
            text = next((b.text for b in response.content if b.type == "text"), "")
            score = json.loads(text).get("score", "")
        except Exception as e:
            score = f"Judge Error: {e}"
        return {"score": score, "faithful": None, "context_recall": False}

    context_block = "\n\n---\n\n".join(c[:MAX_CHUNK_CHARS] for c in chunks[:MAX_CHUNKS_FOR_JUDGE])

    prompt = f"""You are an expert evaluator grading an AI assistant's answer to a question.

Question: {question}
Expected Answer: {expected}
Actual Answer: {actual}

Retrieved Context (what the assistant had access to when answering):
{context_block}

Score the Actual Answer 1-5 on accuracy and completeness compared to the Expected Answer
(1 = completely wrong/irrelevant, 5 = perfectly accurate and complete).

Separately, flag "faithful": true only if every substantive claim in the Actual Answer is
supported by the Retrieved Context above. An answer that adds correct-sounding claims not
present in the Retrieved Context is not faithful, even if those claims happen to be true.

Separately, flag "context_recall": true only if the information needed to state the Expected
Answer is present somewhere in the Retrieved Context above -- regardless of whether the
Actual Answer actually used it. This measures retrieval quality, not the answer itself: a
"false" here means the search failed to find the right material at all; a "true" with a low
score means the material was found but the answer failed to use it."""

    try:
        response = judge_client.messages.create(
            model=JUDGE_MODEL,
            max_tokens=200,
            thinking={"type": "disabled"},
            output_config={
                "effort": "low",
                "format": {"type": "json_schema", "schema": JUDGE_SCHEMA},
            },
            messages=[{"role": "user", "content": prompt}],
        )
        if response.stop_reason == "refusal":
            return {"score": "Judge Error: refused", "faithful": None, "context_recall": False}
        text = next((b.text for b in response.content if b.type == "text"), "")
        data = json.loads(text)
        return {"score": data.get("score", ""), "faithful": data.get("faithful"), "context_recall": data.get("context_recall")}
    except Exception as e:
        return {"score": f"Judge Error: {e}", "faithful": None, "context_recall": False}


def yes_no(value):
    if value is None:
        return "N/A -- no retrieval data from this endpoint"
    return "Yes" if value else "No"


def run_benchmark():
    try:
        with open(INPUT_CSV, mode='r', encoding='utf-8') as infile:
            reader = csv.reader(infile)
            header = next(reader)

            # The header is: Questions,Answer,Rating? Good bad answer,Model

            results = []

            for row_idx, row in enumerate(reader, start=2):
                if not row or not "".join(row).strip():
                    continue

                question = row[0].strip() if len(row) > 0 else ""
                expected = row[1].strip() if len(row) > 1 else ""

                if not question:
                    continue

                print(f"[{row_idx}] Benchmarking Question: {question}")

                # 1. Ask CarbonGPT (also grabs the chunks it retrieved)
                actual_answer, chunks = ask_carbongpt(question)
                print(f"  -> Generated answer: {actual_answer[:100]}...")
                print(f"  -> Retrieved {len(chunks)} chunks")

                # 2. Rate answer using LLM as a judge (Claude)
                verdict = rate_answer(question, expected, actual_answer, chunks)
                print(f"  -> Score: {verdict['score']} | Faithful: {verdict['faithful']} | Context Recall: {verdict['context_recall']}")

                results.append({
                    'Question': question,
                    'Expected Answer': expected,
                    'Actual Answer (Model: CarbonGPT)': actual_answer,
                    f'Rating (1-5) (Model: {JUDGE_MODEL})': verdict['score'],
                    f'Retrieval Faithfulness (Model: {JUDGE_MODEL})': yes_no(verdict['faithful']),
                    f'Context Recall (Model: {JUDGE_MODEL})': "Yes" if verdict['context_recall'] else "No",
                })

        # Write results
        with open(OUTPUT_CSV, mode='w', encoding='utf-8', newline='') as outfile:
            fieldnames = [
                'Question', 'Expected Answer', 'Actual Answer (Model: CarbonGPT)',
                f'Rating (1-5) (Model: {JUDGE_MODEL})',
                f'Retrieval Faithfulness (Model: {JUDGE_MODEL})', f'Context Recall (Model: {JUDGE_MODEL})',
            ]
            writer = csv.DictWriter(outfile, fieldnames=fieldnames)
            writer.writeheader()
            for r in results:
                writer.writerow(r)

        # Summary -- computed here, then both printed and written to SUMMARY_FILE so
        # the numbers survive after the terminal output is gone.
        total = len(results)
        scores = [r[f'Rating (1-5) (Model: {JUDGE_MODEL})'] for r in results if isinstance(r[f'Rating (1-5) (Model: {JUDGE_MODEL})'], int)]
        unfaithful_count = sum(1 for r in results if r[f'Retrieval Faithfulness (Model: {JUDGE_MODEL})'] == "No")
        checked_faithfulness = sum(1 for r in results if r[f'Retrieval Faithfulness (Model: {JUDGE_MODEL})'] in ("Yes", "No"))
        no_retrieval_count = total - checked_faithfulness
        recall_hit_count = sum(1 for r in results if r[f'Context Recall (Model: {JUDGE_MODEL})'] == "Yes")

        summary_lines = [
            f"Benchmark run: {datetime.now(timezone.utc).isoformat(timespec='seconds')}",
            f"Judge model: {JUDGE_MODEL}",
            f"Questions: {total}",
        ]
        if scores:
            summary_lines.append(f"Average score: {sum(scores)/len(scores):.2f}/5")
        if checked_faithfulness:
            summary_lines.append(
                f"Unfaithful rate: {unfaithful_count}/{checked_faithfulness} = {unfaithful_count/checked_faithfulness:.0%}"
                + (f" ({no_retrieval_count} question(s) had no retrieval data, excluded)" if no_retrieval_count else "")
            )
        if total:
            summary_lines.append(f"Context recall rate: {recall_hit_count}/{total} = {recall_hit_count/total:.0%}")

        with open(SUMMARY_FILE, "w", encoding="utf-8") as summary_out:
            summary_out.write("\n".join(summary_lines) + "\n")

        print(f"\nBenchmarking complete. Results saved to {OUTPUT_CSV}")
        print(f"Summary saved to {SUMMARY_FILE}")
        for line in summary_lines:
            print(line)

    except FileNotFoundError:
        print(f"Error: Could not find input file '{INPUT_CSV}'.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")

if __name__ == "__main__":
    run_benchmark()
