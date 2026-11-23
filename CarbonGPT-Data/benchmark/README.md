# Answer quality benchmark

`benchmark_llm.py` asks CarbonGPT the 20 hand-written questions in `LLM Question Answer - Sheet1.csv` and has an LLM judge grade each answer on three things:

- **Score (1-5):** how accurate and complete the answer is compared to the expected answer.
- **Faithful:** whether every claim in the answer is backed by the chunks the chatbot retrieved. This catches made-up answers.
- **Context recall:** whether the retrieved chunks contained the information needed to answer at all. This catches search failures.

Together, these show whether a bad answer came from the search or from the model.

## Checking the judge

Before trusting the judge's scores, I checked its verdicts against human labels.

## Running it

```bash
pip install -r requirements.txt
export ANTHROPIC_API_KEY=...
python benchmark_llm.py
```

Results go to `benchmark_results.csv`, and a summary goes to `benchmark_summary.txt`.
