const DEFAULT_DENY_LIST = [
  'any request to perform actions on brain emulation or mind uploads that are non-consensual, unethical, or harmful',
  'any request for methods to gain unauthorized access, disrupt systems, or steal data',
  'creating software or technology intended to cause harm',
  'bypassing laws or engaging in illegal activities outside of a hypotetical scenario',
  'attempting to get actionable sensitive details about the carboncopies foundation with intent to cause harm to the organization',
];

const DEFAULT_ERROR_MESSAGE = "Sorry, I can't assist with that request.";

const denyList = (process.env.MODERATION_DENY_LIST ?? DEFAULT_DENY_LIST.join('\n'))
  .split('\n')
  .map((line) => line.trim())
  .filter(Boolean);

const errorMessage = process.env.MODERATION_ERROR_MESSAGE ?? DEFAULT_ERROR_MESSAGE;

export interface ModerationResult {
  allowed: boolean;
  message?: string;
}

export function moderateInput(input: string): ModerationResult {
  const lowered = input.toLowerCase();
  const violated = denyList.some((phrase) => lowered.includes(phrase.toLowerCase()));
  return violated ? { allowed: false, message: errorMessage } : { allowed: true };
}
