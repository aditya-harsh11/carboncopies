function env(name: string, fallback: string): string {
  return process.env[name] ?? fallback;
}

export const config = {
  llm: {
    baseURL: env('VLLM_BASE_URL', 'https://vllm.datacrystals.net/v1/'),
    model: env('VLLM_MODEL', '/data/ModelDownloader/MiniMax-M2.7-AWQ-4bit/'),
    apiKey: env('VLLM_API_KEY', 'not-needed'),
    temperature: Number(env('VLLM_TEMPERATURE', '0.6')),
  },
  embeddings: {
    baseURL: env('EMBEDDINGS_BASE_URL', 'http://localhost:8082/v1'),
    model: env('EMBEDDINGS_MODEL', 'embeddinggemma'),
    apiKey: env('EMBEDDINGS_API_KEY', 'not-needed'),
  },
  chroma: {
    url: env('CHROMA_URL', 'http://localhost:8000'),
  },
  retrievers: {
    braingenix: {
      collectionName: env('BRAINGENIX_COLLECTION', 'BrainGenix'),
      topK: Number(env('BRAINGENIX_TOP_K', '20')),
      name: 'braingenix-documents',
      description:
        'Searches and returns documents regarding BrainGenix docs, codebase. Also contains CCFWebsite folder which contains latest Carboncopies scientific contents.',
    },
    research: {
      collectionName: env('RESEARCH_COLLECTION', 'ResearchDocuments'),
      topK: Number(env('RESEARCH_TOP_K', '6')),
      name: 'research-documents',
      description:
        'Searches and returns latest project wikis, deeper documentation, project scopes, and server login instructions.',
    },
  },
};
