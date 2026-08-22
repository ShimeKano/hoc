import fs from 'node:fs/promises';
import path from 'node:path';

const CONFIG_PATH = process.env.HOC_AI_CONFIG || path.join(process.cwd(), 'data', 'apis.json');
const DEFAULT_CONFIG = { apis: [] };

function normalizeConfig(value) {
  const apis = Array.isArray(value) ? value : value?.apis;
  if (!Array.isArray(apis)) throw new Error('Config must be an array or an object with an "apis" array.');
  return { apis: apis.map((item, index) => {
    if (!item || typeof item !== 'object') throw new Error(`API #${index + 1} must be an object.`);
    const provider = String(item.provider || 'auto').toLowerCase();
    const key = String(item.key || '').trim();
    if (!key) throw new Error(`API #${index + 1} is missing "key".`);
    if (!['auto', 'openai', 'openrouter'].includes(provider)) throw new Error(`API #${index + 1} has unsupported provider "${provider}".`);
    return { id: String(item.id || `api-${index + 1}`), name: String(item.name || `${provider}-${index + 1}`), provider, key, enabled: item.enabled !== false };
  }) };
}

export async function readConfig() {
  try { return normalizeConfig(JSON.parse(await fs.readFile(CONFIG_PATH, 'utf8'))); }
  catch (error) { if (error.code === 'ENOENT') return DEFAULT_CONFIG; throw error; }
}

export async function writeConfig(value) {
  const config = normalizeConfig(value);
  await fs.mkdir(path.dirname(CONFIG_PATH), { recursive: true });
  await fs.writeFile(CONFIG_PATH, `${JSON.stringify(config, null, 2)}\n`, 'utf8');
  return config;
}

export function publicConfig(config) {
  return { apis: config.apis.map(({ key, ...api }) => ({ ...api, keyPreview: key.length > 10 ? `${key.slice(0, 6)}…${key.slice(-4)}` : '••••••', hasKey: Boolean(key) })) };
}

export function inferProvider(api) {
  if (api.provider !== 'auto') return api.provider;
  if (api.key.startsWith('sk-or-')) return 'openrouter';
  if (api.key.startsWith('sk-')) return 'openai';
  return null;
}

export { CONFIG_PATH };
