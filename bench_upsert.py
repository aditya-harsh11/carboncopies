"""
Benchmark harness for the Upsert.py ingestion pipeline.

Times each stage of the pipeline SEPARATELY so we can see where the time
actually goes, instead of guessing:

    walk+read  ->  clean_content  ->  chunk  ->  embed  ->  chroma upsert

It imports the real functions out of Upsert.py, so it measures the shipping
code path rather than a reimplementation of it.

READ-ONLY with respect to the real database: the embed/upsert stage writes to
a scratch Chroma path that you pass in explicitly. It never touches ./chroma.

Usage
-----
    # Stage 1: CPU-only stages over the whole dataset (no server needed).
    python bench_upsert.py cpu --dataset ./Dataset

    # Stage 2: embedding + upsert throughput on a sample (server required).
    python bench_upsert.py embed --dataset ./Dataset --sample 500 --batch 10
"""

import argparse
import os
import re
import statistics
import sys
import time
from collections import Counter

import Upsert

# Mirrors the filters inside Upsert.process_directory_to_collection so the
# harness walks exactly the same file set the real pipeline does.
IGNORE_DIRS = ['.git', '__pycache__', 'node_modules', '.idea', 'thirdparty', 'third_party']
IGNORE_FILES = ['.gitignore', 'package-lock.json', 'yarn.lock']
INDEXED_EXTS = ['.h', '.cpp', '.json', '.py', '.md', '.txt', '.cs', '.java', '.js', '.ts', '.html', '.css']
CODE_EXTS = ['.h', '.cpp', '.py', '.java', '.cs', '.ts']

# The vendor/build-artifact signature from the 38.8% finding (Dev Notes 6.6c).
JUNK_RE = re.compile(
    r'(\.min\.(js|css)|/assets/javascripts/|/assets/stylesheets/|'
    r'/material/templates/|lunr|iconsearch_index)',
    re.I,
)


class Stage:
    """Accumulates elapsed time for one pipeline stage."""

    def __init__(self, name):
        self.name = name
        self.elapsed = 0.0

    def __enter__(self):
        self._t0 = time.perf_counter()
        return self

    def __exit__(self, *exc):
        self.elapsed += time.perf_counter() - self._t0
        return False


def iter_indexed_files(dataset_path):
    """Yield every file the real pipeline would index, applying its filters."""
    for collection in sorted(os.listdir(dataset_path)):
        subdir = os.path.join(dataset_path, collection)
        if not os.path.isdir(subdir):
            continue
        for root, dirs, files in os.walk(subdir):
            if any(p.lower() in ('thirdparty', 'third_party') for p in root.split(os.sep)):
                continue
            dirs[:] = [d for d in dirs if d.lower() not in [x.lower() for x in IGNORE_DIRS]]
            for name in files:
                if name.lower() in [f.lower() for f in IGNORE_FILES]:
                    continue
                ext = os.path.splitext(name)[1].lower()
                if ext not in INDEXED_EXTS:
                    continue
                yield collection, os.path.join(root, name), ext


def run_cpu(dataset_path):
    """Time the CPU-bound stages over the whole dataset. No server needed."""
    read_s = Stage('read')
    clean_s = Stage('clean')
    chunk_s = Stage('chunk')

    files = 0
    skipped = 0
    total_bytes = 0
    total_chunks = 0
    junk_chunks = 0
    chunks_by_collection = Counter()
    chunks_by_ext = Counter()
    chunk_lengths = []

    wall0 = time.perf_counter()

    for collection, path, ext in iter_indexed_files(dataset_path):
        try:
            with read_s:
                with open(path, 'r', encoding='utf-8') as fh:
                    content = fh.read()
        except (UnicodeDecodeError, OSError):
            skipped += 1
            continue

        files += 1
        total_bytes += len(content)

        with clean_s:
            cleaned = Upsert.clean_content(content, ext)

        if not cleaned.strip():
            continue

        with chunk_s:
            if ext in CODE_EXTS:
                chunks = Upsert.chunk_code_file(cleaned, ext)
            else:
                chunks = Upsert.chunk_text_file(cleaned)

        kept = [c for c in chunks if c.strip()]
        total_chunks += len(kept)
        chunks_by_collection[collection] += len(kept)
        chunks_by_ext[ext] += len(kept)
        chunk_lengths.extend(len(c) for c in kept)
        if JUNK_RE.search(path):
            junk_chunks += len(kept)

    wall = time.perf_counter() - wall0

    print()
    print('=' * 68)
    print('PHASE 1 BASELINE - CPU stages over the full dataset')
    print('=' * 68)
    print(f'files indexed      : {files:,}   (skipped/binary: {skipped:,})')
    print(f'bytes read         : {total_bytes:,}')
    print(f'chunks produced    : {total_chunks:,}')
    if chunk_lengths:
        print(f'chunk length       : mean {statistics.mean(chunk_lengths):.0f} chars, '
              f'median {statistics.median(chunk_lengths):.0f}, max {max(chunk_lengths):,}')
    print()
    print(f'{"stage":<20}{"seconds":>12}{"% of CPU work":>16}')
    print('-' * 48)
    cpu_total = read_s.elapsed + clean_s.elapsed + chunk_s.elapsed
    for st in (read_s, clean_s, chunk_s):
        pct = (st.elapsed / cpu_total * 100) if cpu_total else 0
        print(f'{st.name:<20}{st.elapsed:>12.2f}{pct:>15.1f}%')
    print('-' * 48)
    print(f'{"CPU TOTAL":<20}{cpu_total:>12.2f}')
    print(f'{"wall clock":<20}{wall:>12.2f}')
    print()
    print(f'junk chunks (6.6c) : {junk_chunks:,} / {total_chunks:,} '
          f'({junk_chunks / total_chunks * 100:.1f}%)' if total_chunks else '')
    print()
    print('chunks by collection:')
    for name, n in chunks_by_collection.most_common():
        print(f'  {n:>8,}  {name}')
    print()
    print('chunks by extension (top 10):')
    for ext, n in chunks_by_ext.most_common(10):
        print(f'  {n:>8,}  {ext}')

    return total_chunks


def collect_chunks(dataset_path, limit):
    """Gather up to `limit` real chunks to feed the embedding benchmark."""
    out = []
    for _collection, path, ext in iter_indexed_files(dataset_path):
        try:
            with open(path, 'r', encoding='utf-8') as fh:
                content = fh.read()
        except (UnicodeDecodeError, OSError):
            continue
        cleaned = Upsert.clean_content(content, ext)
        if not cleaned.strip():
            continue
        chunks = Upsert.chunk_code_file(cleaned, ext) if ext in CODE_EXTS else Upsert.chunk_text_file(cleaned)
        for c in chunks:
            if c.strip():
                out.append(c)
                if len(out) >= limit:
                    return out
    return out


def run_embed(dataset_path, api_url, sample, batch_size, total_chunks_hint, workers=1):
    """Measure embedding throughput at a given batch size, then extrapolate."""
    chunks = collect_chunks(dataset_path, sample)
    if not chunks:
        print('No chunks collected; aborting.')
        return

    ef = Upsert.LlamaCppEmbeddingFunction(api_base_url=api_url, model_name='embedding-model')

    # Warm the server so the first batch does not absorb model load time.
    ef(['warmup'])

    batches = [chunks[i:i + batch_size] for i in range(0, len(chunks), batch_size)]
    per_batch = []

    if workers > 1:
        from concurrent.futures import ThreadPoolExecutor

        def timed(b):
            bt = time.perf_counter()
            ef(b)
            return time.perf_counter() - bt

        t0 = time.perf_counter()
        with ThreadPoolExecutor(max_workers=workers) as pool:
            per_batch = list(pool.map(timed, batches))
        elapsed = time.perf_counter() - t0
    else:
        t0 = time.perf_counter()
        for b in batches:
            bt = time.perf_counter()
            ef(b)
            per_batch.append(time.perf_counter() - bt)
        elapsed = time.perf_counter() - t0

    rate = len(chunks) / elapsed
    print()
    print('=' * 68)
    print(f'EMBEDDING THROUGHPUT - batch_size={batch_size}, workers={workers}')
    print('=' * 68)
    print(f'chunks embedded    : {len(chunks):,}')
    print(f'requests made      : {len(batches):,}')
    print(f'elapsed            : {elapsed:.2f} s')
    print(f'throughput         : {rate:.1f} chunks/s')
    print(f'per-request        : mean {statistics.mean(per_batch)*1000:.0f} ms, '
          f'median {statistics.median(per_batch)*1000:.0f} ms')
    if total_chunks_hint:
        projected = total_chunks_hint / rate
        print()
        print(f'PROJECTED for {total_chunks_hint:,} chunks: '
              f'{projected/60:.1f} min ({projected:.0f} s)')
    return rate


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('stage', choices=['cpu', 'embed'])
    ap.add_argument('--dataset', default='./Dataset')
    ap.add_argument('--api-url', default='http://localhost:8082')
    ap.add_argument('--sample', type=int, default=500)
    ap.add_argument('--batch', type=int, default=10)
    ap.add_argument('--workers', type=int, default=1)
    ap.add_argument('--total-chunks', type=int, default=0,
                    help='Chunk count from the cpu stage, used to project full-run time.')
    args = ap.parse_args()

    if not os.path.isdir(args.dataset):
        print(f'Dataset not found: {args.dataset}')
        sys.exit(1)

    if args.stage == 'cpu':
        run_cpu(args.dataset)
    else:
        run_embed(args.dataset, args.api_url, args.sample, args.batch, args.total_chunks, args.workers)


if __name__ == '__main__':
    main()
