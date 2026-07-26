import os
import chromadb
import uuid
import hashlib
import requests
import json
import re
import numpy as np
from typing import List, Dict, Any, Optional, Tuple
from chromadb.api.types import Documents, EmbeddingFunction, Embeddings

def get_embedding_dimension(embedding_function: EmbeddingFunction, sample_text: str = "test") -> Optional[int]:
    """
    Get the embedding dimension from an embedding function by testing it with sample text.
    """
    try:
        test_embeddings = embedding_function([sample_text])
        if test_embeddings and len(test_embeddings) > 0:
            first_emb = test_embeddings[0]
            if isinstance(first_emb, (list, np.ndarray)):
                return len(first_emb)
            elif isinstance(first_emb, (int, float)):
                return 1
        return None
    except Exception as e:
        print(f"Error getting embedding dimension: {e}")
        return None


def get_collection_embedding_dimension(collection) -> Optional[int]:
    """
    Get the embedding dimension from an existing ChromaDB collection.
    """
    try:
        # Try to peek at existing embeddings
        peek = collection.peek(limit=1)
        if peek and 'embeddings' in peek and peek['embeddings'] is not None:
            embeddings = peek['embeddings']
            # Handle both list and numpy array
            if isinstance(embeddings, np.ndarray):
                if embeddings.size > 0:
                    # 2D array: [num_embeddings, embedding_dim]
                    return embeddings.shape[1]
            elif isinstance(embeddings, list) and len(embeddings) > 0:
                first_emb = embeddings[0]
                if isinstance(first_emb, (list, np.ndarray)):
                    return len(first_emb)
                elif isinstance(first_emb, (int, float)):
                    return 1
        return None
    except Exception as e:
        print(f"Error getting collection embedding dimension: {e}")
        import traceback
        traceback.print_exc()
        return None


def generate_id(filepath, chunk_index):
    """
    Generates a unique and stable ID for each document chunk.
    This ensures that upsert operations are consistent.
    """
    content_hash = hashlib.sha256(f"{filepath}:{chunk_index}".encode()).hexdigest()
    return str(uuid.uuid5(uuid.NAMESPACE_URL, content_hash))

class LlamaCppEmbeddingFunction(EmbeddingFunction[Documents]):
    """
    Custom embedding function to connect to a running llama.cpp server.
    """
    def __init__(self, api_base_url: str, model_name: str = "embedding-model"):
        self.api_base_url = api_base_url
        self.model_name = model_name
        print(f"Using llama.cpp server at {self.api_base_url} with model: {self.model_name}")

    def __call__(self, input: Documents) -> Embeddings:
        """
        Calls the llama.cpp embeddings API for a list of documents.
        Returns a list of embeddings (one per input document).
        """
        # Handle empty input - return zero vectors of the expected dimension
        if not input or len(input) == 0:
            # If we know the expected dimension from a previous call, use it
            # Otherwise, default to 768 (common dimension)
            dim = getattr(self, 'last_dimension', 768)
            print(f"Warning: Empty input to embedding function, returning zero vectors of dimension {dim}")
            # Always return a list, even for empty input
            return [[0.0] * dim for _ in range(len(input))] if input else [[]]
        
        payload = {
            "model": self.model_name,
            "input": input
        }
        
        try:
            # The OpenAI-compatible API supports a list of inputs
            response = requests.post(
                f"{self.api_base_url}/v1/embeddings",
                json=payload,
                timeout=120  # Increased timeout to 2 minutes for large batches
            )
            
            # Check for token limit errors first
            if response.status_code == 400:
                error_data = response.json()
                error_msg = error_data.get('error', {}).get('message', '')
                if 'context size' in error_msg.lower() or 'too large' in error_msg.lower():
                    # Token limit exceeded - this chunk is too big
                    print(f"Token limit error: {error_msg}")
                    raise ValueError(f"Input too large for model context window: {error_msg}")
            
            response.raise_for_status()  # Raise an HTTPError for bad responses (4xx or 5xx)
            
            data = response.json()
            
            # Check for the expected data structure
            if "data" not in data:
                print(f"API response missing 'data' key. Full response: {data}")
                raise ValueError(f"Invalid API response: missing 'data' key")
            
            if not isinstance(data["data"], list):
                print(f"API response 'data' is not a list. Type: {type(data['data'])}")
                raise ValueError(f"Invalid API response: 'data' is not a list")
            
            # Extract the embeddings from the response
            embeddings = []
            for i, item in enumerate(data["data"]):
                if "embedding" not in item:
                    print(f"Warning: Item {i} missing 'embedding' key, using zero vector")
                    # Use a zero vector as fallback with expected dimension
                    dim = getattr(self, 'last_dimension', 768)
                    embeddings.append([0.0] * dim)
                else:
                    embeddings.append(item["embedding"])
            
            # Validate embeddings - if empty, use zero vectors
            if len(embeddings) == 0:
                print(f"Warning: Got empty embeddings list, creating zero vectors")
                dim = getattr(self, 'last_dimension', 768)
                embeddings = [[0.0] * dim for _ in range(len(input))]
            elif len(embeddings) != len(input):
                print(f"Error: Expected {len(input)} embeddings, got {len(embeddings)}")
                # Pad with zero vectors if we have fewer than expected
                dim = getattr(self, 'last_dimension', 768)
                while len(embeddings) < len(input):
                    embeddings.append([0.0] * dim)
            
            # Check for None or invalid values
            for i, emb in enumerate(embeddings):
                if emb is None:
                    print(f"Error: Embedding {i} is None")
                    raise ValueError(f"Embedding {i} is None")
                if not isinstance(emb, (list, np.ndarray)):
                    print(f"Error: Embedding {i} is not a list or array, type: {type(emb)}")
                    raise ValueError(f"Embedding {i} has invalid type: {type(emb)}")
            
            # Log embedding dimension for debugging
            if len(embeddings) > 0 and len(embeddings[0]) > 0:
                emb_dim = len(embeddings[0])
                if emb_dim not in [384, 768, 1024]:
                    print(f"Note: Embedding dimension is {emb_dim}")
                self.last_dimension = emb_dim
                
        except requests.exceptions.RequestException as e:
            print(f"Error calling llama.cpp API: {e}")
            if e.response:
                print(f"Status code: {e.response.status_code}")
                print(f"Response: {e.response.text[:1000]}")
            else:
                print(f"No response received")
            print(f"Payload preview: {str(payload)[:500]}")
            
            # For 400 errors, provide more specific guidance
            if e.response and e.response.status_code == 400:
                error_msg = e.response.text.lower() if e.response.text else ""
                if "token" in error_msg or "length" in error_msg:
                    print("  ⚠️  Input too long! Try reducing batch size or content length.")
                elif "invalid" in error_msg or "format" in error_msg:
                    print("  ⚠️  Invalid input format! Check for special characters or HTML tags.")
                else:
                    print("  ⚠️  Bad request. Check the API server logs for details.")
            
            raise RuntimeError(f"Failed to call embedding API: {e}")
        except json.JSONDecodeError as e:
            print(f"Error decoding JSON response: {e}")
            raise RuntimeError(f"Invalid JSON response from embedding API")
        except Exception as e:
            print(f"Error in embedding function: {e}")
            import traceback
            traceback.print_exc()
            raise
            
        return embeddings

def clean_content(content, file_extension):
    """
    Conservative cleaning that preserves code structure and syntax.
    Only removes boilerplate, not actual content or syntax.
    """
    # Remove tool-related patterns (like the example provided)
    tool_pattern = r'tool:"[^"]*"\s*toolInput:\{[^}]*\}\s*toolOutput:"[^"]*"'
    content = re.sub(tool_pattern, '', content, flags=re.DOTALL)
    
    # Remove BrainGenix header pattern
    header_pattern = r'//=================================//\s*// This file is part of BrainGenix\s*//=================================//'
    content = re.sub(header_pattern, '', content)
    
    # For C++ files, only remove license blocks, not all comments
    if file_extension in ['.h', '.cpp']:
        # Remove license blocks (common patterns)
        license_patterns = [
            r'/\*.*?GNU Affero General Public License.*?\*/',
            r'//.*?GNU Affero General Public License.*?\n',
            r'/\*.*?Copyright \(C\) \d{4}.*?\*/',
            r'//.*?Copyright \(C\) \d{4}.*?\n'
        ]
        
        for pattern in license_patterns:
            content = re.sub(pattern, '', content, flags=re.DOTALL)
    
    # Remove metadata patterns
    metadata_patterns = [
        r'Description:.*?Additional Notes: None',
        r'Date Created: \d{4}-\d{2}-\d{2}',
        r'Author\(s\): [^\n]+'
    ]
    
    for pattern in metadata_patterns:
        content = re.sub(pattern, '', content, flags=re.DOTALL)
    
    # For Markdown files ONLY, remove problematic HTML/Markdown that causes API 400 errors
    if file_extension == '.md':
        # Remove HTML iframe tags (common cause of 400 errors)
        content = re.sub(r'<iframe[^>]*>.*?</iframe>', '', content, flags=re.DOTALL | re.IGNORECASE)
        # Remove other HTML tags but keep the text content
        content = re.sub(r'<[^>]+>', '', content)
        # Remove Markdown images (they reference local files that don't exist)
        content = re.sub(r'!\[[^\]]*\]\([^)]*\)', '', content)
    
    # Remove multiple empty lines but preserve code structure
    content = re.sub(r'\n\s*\n', '\n', content)
    
    # Remove leading/trailing whitespace
    content = content.strip()
    
    return content

def process_directory_to_collection(directory_path: str, collection_name: str, client, embedding_function):
    """
    Processes a directory and upserts its text files into a ChromaDB collection.
    
    Args:
        directory_path (str): The path to the directory to process.
        collection_name (str): The name of the ChromaDB collection.
        client: The ChromaDB client instance.
        embedding_function: The embedding function to use.
    """
    try:
        # Check if collection exists and get its embedding dimension
        existing_collections = client.list_collections()
        collection_exists = any(
            (col.name if hasattr(col, 'name') else col) == collection_name 
            for col in existing_collections
        )
        
        collection = None
        if collection_exists:
            # Collection exists - check dimension compatibility
            collection = client.get_collection(name=collection_name)
            existing_dim = get_collection_embedding_dimension(collection)
            ef_dim = get_embedding_dimension(embedding_function)
            
            print(f"Collection '{collection_name}' exists with embedding dimension: {existing_dim}")
            print(f"Embedding function produces dimension: {ef_dim}")
            
            if existing_dim and ef_dim and existing_dim != ef_dim:
                print(f"WARNING: Dimension mismatch! Collection expects {existing_dim}, but embedding function produces {ef_dim}")
                print(f"To fix this, you need to either:")
                print(f"  1. Use a different embedding model that produces {existing_dim}-dimensional embeddings")
                print(f"  2. Delete and recreate the collection with the current model")
                print(f"  3. Implement embedding dimension conversion")
                print(f"\nFor now, attempting to continue with existing collection...")
        
        # Get or create the collection, passing the embedding function.
        if collection is None:
            collection = client.get_or_create_collection(name=collection_name, embedding_function=embedding_function)
        else:
            # Collection exists, use it without reassigning embedding function
            # (to avoid dimension conflicts)
            pass
        
        # Lists to store the documents, metadata, and ids
        documents_to_upsert = []
        metadatas_to_upsert = []
        ids_to_upsert = []
        
        # Define files and directories to ignore (case-insensitive)
        ignore_dirs = ['.git', '__pycache__', 'node_modules', '.idea', 'thirdparty', 'third_party']
        ignore_files = ['.gitignore', 'package-lock.json', 'yarn.lock']
        
        # Walk through the directory
        for root, dirs, files in os.walk(directory_path):
            # Skip ThirdParty directories (case-insensitive)
            # Check if any part of the current path contains a ThirdParty directory
            path_parts = root.split(os.sep)
            if any(part.lower() in ['thirdparty', 'third_party'] for part in path_parts):
                continue
                
            # Exclude directories from the ignore list (case-insensitive)
            dirs[:] = [d for d in dirs if d.lower() not in [x.lower() for x in ignore_dirs]]
            
            for file in files:
                # Skip ignored files
                if file.lower() in [f.lower() for f in ignore_files]:
                    continue
                    
                # Get file extension
                _, file_extension = os.path.splitext(file)
                
                # Only process specific file extensions
                if file_extension.lower() not in ['.h', '.cpp', '.json', '.py', '.md', '.txt', '.cs', '.java', '.js', '.ts', '.html', '.css']:
                    continue
                    
                # Construct the full file path
                filepath = os.path.join(root, file)
                
                try:
                    # Print the file being indexed
                    print(f"Indexing file: {filepath}")
                    
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read()
                    
                    # Clean the content to remove boilerplate
                    cleaned_content = clean_content(content, file_extension.lower())
                    
                    # Skip if the cleaned content is empty
                    if not cleaned_content.strip():
                        print(f"Skipping empty file after cleaning: {filepath}")
                        continue
                        
                    # Use semantic chunking for code files (except .js which is often minified)
                    if file_extension.lower() in ['.h', '.cpp', '.py', '.java', '.cs', '.ts']:
                        # For code files, try to chunk by functions/classes when possible
                        chunks = chunk_code_file(cleaned_content, file_extension.lower())
                        print(f"  Using semantic chunking for {file_extension}")
                    else:
                        # For other files (including .js, .md, .txt, etc.), use character-based chunking
                        chunks = chunk_text_file(cleaned_content)
                        print(f"  Using text chunking for {file_extension}, chunk_size=350")
                    
                    # Generate chunks
                    chunk_count = 0
                    for i, chunk in enumerate(chunks):
                        # Skip empty chunks
                        if not chunk.strip():
                            continue
                            
                        # Create a unique ID for the chunk
                        doc_id = generate_id(filepath, i)
                        
                        # Append to the lists
                        documents_to_upsert.append(chunk)
                        metadatas_to_upsert.append({
                            "source": filepath,
                            "chunk_index": i,
                            "file_name": file,
                            "collection": collection_name,
                            "file_type": file_extension.lower()
                        })
                        ids_to_upsert.append(doc_id)
                        chunk_count += 1
                    
                    print(f"Created {chunk_count} chunks from {filepath}")
                
                except UnicodeDecodeError:
                    print(f"Skipping binary file: {filepath}")
                    continue
                except Exception as e:
                    print(f"Error processing file {filepath}: {e}")
                    continue

        if documents_to_upsert:
            # Upsert the documents in batches to avoid server timeouts
            batch_size = 10  # Default batch size
            total_batches = (len(documents_to_upsert) + batch_size - 1) // batch_size
            
            successful_batches = 0
            failed_batches = 0
            
            for i in range(0, len(documents_to_upsert), batch_size):
                batch_documents = documents_to_upsert[i:i + batch_size]
                batch_metadatas = metadatas_to_upsert[i:i + batch_size]
                batch_ids = ids_to_upsert[i:i + batch_size]

                print(f"Upserting batch {i//batch_size + 1}/{total_batches} with {len(batch_documents)} chunks for collection '{collection_name}'...")
                try:
                    # Validate batch
                    if not batch_documents:
                        print(f"Warning: Empty batch, skipping")
                        continue
                    
                    # Ensure all required fields are present
                    if len(batch_documents) != len(batch_metadatas) or len(batch_documents) != len(batch_ids):
                        print(f"Error: Batch size mismatch - docs: {len(batch_documents)}, metas: {len(batch_metadatas)}, ids: {len(batch_ids)}")
                        failed_batches += 1
                        continue
                    
                    # Check for empty documents in the batch
                    empty_docs = [j for j, doc in enumerate(batch_documents) if not doc or not doc.strip()]
                    if empty_docs:
                        print(f"Warning: Batch contains {len(empty_docs)} empty documents at indices: {empty_docs}")
                        # Filter out empty documents
                        batch_documents = [doc for j, doc in enumerate(batch_documents) if j not in empty_docs]
                        batch_metadatas = [meta for j, meta in enumerate(batch_metadatas) if j not in empty_docs]
                        batch_ids = [id_ for j, id_ in enumerate(batch_ids) if j not in empty_docs]
                        if not batch_documents:
                            print(f"  All documents were empty, skipping batch")
                            continue
                    
                    # Retry logic with exponential backoff
                    max_retries = 3
                    for retry in range(max_retries):
                        try:
                            collection.upsert(
                                documents=batch_documents,
                                metadatas=batch_metadatas,
                                ids=batch_ids
                            )
                            print(f"  Batch upserted successfully")
                            successful_batches += 1
                            break
                        except Exception as inner_e:
                            if retry == max_retries - 1:
                                # Last retry failed - skip this batch
                                print(f"Warning: Skipping batch after {max_retries} failures: {str(inner_e)[:200]}")
                                print(f"  Batch contained {len(batch_documents)} documents from:")
                                # Show which files are in this batch
                                unique_sources = set(m.get('source', 'unknown') for m in batch_metadatas)
                                for source in list(unique_sources)[:3]:
                                    print(f"    - {source}")
                                if len(unique_sources) > 3:
                                    print(f"    ... and {len(unique_sources) - 3} more")
                                failed_batches += 1
                            else:
                                # Wait before retrying
                                import time
                                wait_time = 2 ** retry  # Exponential backoff: 2, 4, 8 seconds
                                print(f"  Retry {retry + 1}/{max_retries} after {wait_time}s...")
                                time.sleep(wait_time)
                except KeyboardInterrupt:
                    print(f"  Batch upsert interrupted by user")
                    failed_batches += 1
                    break
            
            print(f"Upsert complete for collection '{collection_name}': {successful_batches} succeeded, {failed_batches} failed.")
            if failed_batches > 0:
                print(f"  WARNING: {failed_batches} batches failed. Try running again to retry.")
        else:
            print(f"No text documents found to upsert for collection '{collection_name}'.")

    except Exception as e:
        print(f"An error occurred processing directory '{directory_path}' for collection '{collection_name}': {e}")

def chunk_code_file(content, file_extension):
    """
    Attempt to chunk code files by semantic structure (functions, classes, etc.)
    Falls back to character-based chunking if semantic chunking fails.
    """
    chunks = []
    
    # Try to split by functions/classes for different languages
    if file_extension in ['.cpp', '.h', '.java', '.cs']:
        # Pattern for C-style languages: look for function/class definitions
        pattern = r'(\b(class|struct|namespace)\s+\w+\s*{|\b\w+\s+\w+\([^)]*\)\s*({|=>))'
        chunks = split_by_pattern(content, pattern)
    elif file_extension == '.py':
        # Pattern for Python: look for def, class, and @ decorators
        pattern = r'(\b(def|class)\s+\w+|\@\w+)'
        chunks = split_by_pattern(content, pattern)
    elif file_extension in ['.js', '.ts']:
        # Pattern for JavaScript/TypeScript
        pattern = r'(\b(function|class|const|let|var)\s+\w+|export\s+default)'
        chunks = split_by_pattern(content, pattern)
    
    # If semantic chunking didn't work well, fall back to character-based
    # Also ensure no chunk exceeds the token limit
    if len(chunks) == 0 or max(len(chunk) for chunk in chunks) > 2000:
        chunks = chunk_text_file(content)
    else:
        # Check if any chunk would exceed token limit
        # Use conservative estimate: 1.5 chars per token for code
        # 512 tokens * 1.5 = 768 chars
        max_chunk_size = 768
        for i, chunk in enumerate(chunks):
            if len(chunk) > max_chunk_size:
                # Fall back to text chunking
                chunks = chunk_text_file(content)
                break
    
    return chunks

def split_by_pattern(content, pattern):
    """
    Split content by a regex pattern while keeping the matches.
    """
    chunks = []
    last_idx = 0
    matches = list(re.finditer(pattern, content))
    
    if not matches:
        return [content]
    
    for i, match in enumerate(matches):
        start_idx = match.start()
        if i > 0:
            chunks.append(content[last_idx:start_idx])
        last_idx = start_idx
    
    # Add the remaining content
    if last_idx < len(content):
        chunks.append(content[last_idx:])
    
    return chunks

def chunk_text_file(content):
    """
    Character-based chunking for non-code files.
    Uses smaller chunks to stay within the embedding model's context window (512 tokens).
    For dense content (minified JS, code), this ensures we stay under the limit.
    """
    # Model has 512 token context window
    # Dense content (minified JS) can be ~1.15-1.28 chars/token
    # 512 tokens ÷ 1.28 ≈ 400 chars (worst case)
    # Use 350 chars with 50 overlap for safety margin to guarantee under 512 tokens
    chunk_size = 350
    overlap = 50  # 50 character overlap for context continuity
    
    chunks = []
    step = chunk_size - overlap
    
    for i in range(0, len(content), step):
        chunk = content[i:i+chunk_size]
        chunks.append(chunk)
    
    return chunks

def upsert_dataset_directories(dataset_path: str, db_path: str, model_name: str = "embedding-model", api_url: str = "http://localhost:8082"):
    """
    Upserts all text files from subdirectories of a dataset path into separate ChromaDB collections.
    Each subdirectory becomes a collection with the same name as the subdirectory.

    Args:
        dataset_path (str): The path to the dataset directory containing subdirectories.
        db_path (str): The path to the ChromaDB persistence directory.
        model_name (str): The name of the embedding model to use.
        api_url (str): The URL of the llama.cpp API server.
    """
    try:
        # Initialize a persistent ChromaDB client
        client = chromadb.PersistentClient(path=db_path)
        
        # Define the embedding function for the local llama.cpp server
        llama_cpp_ef = LlamaCppEmbeddingFunction(api_base_url=api_url, model_name=model_name)
        
        # Test the embedding function to get its dimension
        print(f"\nTesting embedding function with model '{model_name}'...")
        ef_dim = get_embedding_dimension(llama_cpp_ef)
        print(f"Embedding function dimension: {ef_dim}")
        
        if ef_dim is None:
            print("ERROR: Could not determine embedding dimension. Check if the llama.cpp server is running.")
            print(f"Server should be at: {api_url}")
            print("\nTry using the correct model name. Available models on the server:")
            try:
                import requests
                response = requests.get(f"{api_url}/v1/models", timeout=5)
                if response.ok:
                    models = response.json().get('data', [])
                    for m in models:
                        print(f"  - {m.get('id', m.get('name', 'unknown'))}")
            except:
                print("  Could not fetch model list from server")
            return
        
        # Get all subdirectories in the dataset path
        subdirectories = [d for d in os.listdir(dataset_path) 
                         if os.path.isdir(os.path.join(dataset_path, d))]
        
        print(f"Found {len(subdirectories)} subdirectories to process: {subdirectories}")
        
        # Process each subdirectory as a separate collection
        for subdir in subdirectories:
            subdir_path = os.path.join(dataset_path, subdir)
            print(f"\nProcessing directory: {subdir_path} as collection: {subdir}")
            process_directory_to_collection(subdir_path, subdir, client, llama_cpp_ef)
            
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='Upsert documents into ChromaDB collections')
    parser.add_argument('--dataset', type=str, default="./Dataset", help='Path to the dataset directory')
    parser.add_argument('--db', type=str, default="./chroma", help='Path to the ChromaDB persistence directory')
    parser.add_argument('--model', type=str, default="embedding-model", help='Name of the embedding model')
    parser.add_argument('--api-url', type=str, default="http://localhost:8082", help='URL of the llama.cpp API server')
    
    args = parser.parse_args()
    
    # Specify the path to the dataset directory
    dataset_path = args.dataset

    # Specify the path where you want to store the ChromaDB files
    db_persistence_path = args.db
    
    upsert_dataset_directories(dataset_path, db_persistence_path, model_name=args.model, api_url=args.api_url)
