#!/usr/bin/env python3
"""
Upsert only the ResearchDocuments collection into ChromaDB.
This will ADD new documents and UPDATE existing ones without wiping the collection.
"""

import chromadb
from Upsert import process_directory_to_collection, LlamaCppEmbeddingFunction

# Configuration
DATASET_DIR = "./Dataset/ResearchDocuments"
COLLECTION_NAME = "ResearchDocuments"
DB_PATH = "./chroma"
API_URL = "http://localhost:8082"
MODEL_NAME = "embedding-model"

if __name__ == "__main__":
    print(f"Upserting {DATASET_DIR} into collection '{COLLECTION_NAME}'...")
    print(f"Database: {DB_PATH}")
    print(f"API: {API_URL}")
    print()
    
    # Initialize ChromaDB client
    client = chromadb.PersistentClient(path=DB_PATH)
    
    # Initialize embedding function
    ef = LlamaCppEmbeddingFunction(api_base_url=API_URL, model_name=MODEL_NAME)
    
    # Process the directory into the collection
    process_directory_to_collection(
        directory_path=DATASET_DIR,
        collection_name=COLLECTION_NAME,
        client=client,
        embedding_function=ef
    )
    
    print("\nDone!")
