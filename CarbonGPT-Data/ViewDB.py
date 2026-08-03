import chromadb
import random

# Initialize the client
client = chromadb.PersistentClient(path="chroma/")

# 1. Get all collection names
collections = client.list_collections()

if not collections:
    print("No collections found.")
else:
    for col_info in collections:
        # Normalize name (handles different Chroma versions)
        name = col_info if isinstance(col_info, str) else col_info.name
        col = client.get_collection(name=name)
        
        # 2. Pretty print the summary
        count = col.count()
        print(f"\n{'='*60}")
        print(f" COLLECTION: {name}")
        print(f" TOTAL DOCS: {count}")
        print(f"{'='*60}")

        if count == 0:
            print("  [Empty Collection]")
            continue

        # 3. Get all documents and randomly sample 3
        all_data = col.get()
        
        # If there are more than 3 docs, sample randomly; otherwise show all
        if len(all_data["documents"]) > 3:
            indices = random.sample(range(len(all_data["documents"])), 3)
        else:
            indices = range(len(all_data["documents"]))

        for i, idx in enumerate(indices):
            doc = all_data["documents"][idx]
            meta = all_data["metadatas"][idx]
            doc_id = all_data["ids"][idx]
            print(f"\n  [Sample Document {i+1}]")
            print(f"  ID: {doc_id}")
            print(f"  METADATA: {meta}")
            # Display the full content
            print(f"  CONTENT: {doc}")
