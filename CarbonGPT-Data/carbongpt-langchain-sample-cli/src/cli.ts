import { askCarbonGPT } from './agent.js';

const input = process.argv.slice(2).join(' ').trim();

if (!input) {
  console.error('Usage: bun run start "<your question>"');
  process.exit(1);
}

const response = await askCarbonGPT(input);
console.log(response.output);
