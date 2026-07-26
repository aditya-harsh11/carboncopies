import { OpenAIEmbeddings } from '@langchain/openai';
import { Chroma } from '@langchain/community/vectorstores/chroma';
import { createRetrieverTool } from 'langchain/tools/retriever';
import type { StructuredToolInterface } from '@langchain/core/tools';
import { config } from './config.js';

function createEmbeddings(): OpenAIEmbeddings {
  return new OpenAIEmbeddings({
    model: config.embeddings.model,
    apiKey: config.embeddings.apiKey,
    configuration: { baseURL: config.embeddings.baseURL },
  });
}

async function buildRetrieverTool(options: {
  collectionName: string;
  topK: number;
  name: string;
  description: string;
}): Promise<StructuredToolInterface> {
  const vectorStore = new Chroma(createEmbeddings(), {
    collectionName: options.collectionName,
    url: config.chroma.url,
  });

  const retriever = vectorStore.asRetriever(options.topK);

  return createRetrieverTool(retriever, {
    name: options.name,
    description: options.description,
  });
}

export async function buildRetrieverTools(): Promise<StructuredToolInterface[]> {
  return Promise.all([
    buildRetrieverTool(config.retrievers.braingenix),
    buildRetrieverTool(config.retrievers.research),
  ]);
}
