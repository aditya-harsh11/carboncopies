import { ChatOpenAI } from '@langchain/openai';
import { ChatPromptTemplate, MessagesPlaceholder } from '@langchain/core/prompts';
import { AIMessage, HumanMessage, SystemMessage, type BaseMessage } from '@langchain/core/messages';
import { AgentExecutor, createToolCallingAgent } from 'langchain/agents';
import { config } from './config.js';
import { moderateInput } from './moderation.js';
import { buildRetrieverTools } from './retrieverTools.js';
import { SYSTEM_PROMPT } from './systemPrompt.js';

const sessions = new Map<string, BaseMessage[]>();

function getHistory(sessionId: string): BaseMessage[] {
  let history = sessions.get(sessionId);
  if (!history) {
    history = [];
    sessions.set(sessionId, history);
  }
  return history;
}

let executorPromise: Promise<AgentExecutor> | undefined;

function buildExecutor(): Promise<AgentExecutor> {
  if (!executorPromise) {
    executorPromise = (async () => {
      const llm = new ChatOpenAI({
        model: config.llm.model,
        apiKey: config.llm.apiKey,
        temperature: config.llm.temperature,
        streaming: true,
        configuration: { baseURL: config.llm.baseURL },
      });

      const tools = await buildRetrieverTools();

      const prompt = ChatPromptTemplate.fromMessages([
        new SystemMessage(SYSTEM_PROMPT),
        new MessagesPlaceholder('chat_history'),
        ['human', '{input}'],
        new MessagesPlaceholder('agent_scratchpad'),
      ]);

      const agent = await createToolCallingAgent({ llm, tools, prompt });

      return new AgentExecutor({ agent, tools });
    })();
  }
  return executorPromise;
}

export interface CarbonGPTResponse {
  output: string;
  blocked: boolean;
}

export async function askCarbonGPT(
  input: string,
  sessionId = 'default'
): Promise<CarbonGPTResponse> {
  const moderation = moderateInput(input);
  if (!moderation.allowed) {
    return { output: moderation.message ?? 'Request blocked.', blocked: true };
  }

  const history = getHistory(sessionId);
  const executor = await buildExecutor();
  const result = await executor.invoke({ input, chat_history: history });
  const output = String(result['output']);

  history.push(new HumanMessage(input));
  history.push(new AIMessage(output));

  return { output, blocked: false };
}
