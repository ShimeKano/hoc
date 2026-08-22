const OPENAI_MODELS = 'https://api.openai.com/v1/models';
const OPENROUTER_MODELS = 'https://openrouter.ai/api/v1/models';

function headers(api, provider) {
  return { Authorization: `Bearer ${api.key}`, 'Content-Type': 'application/json', ...(provider === 'openrouter' ? { 'HTTP-Referer': process.env.SITE_URL || 'http://localhost:3000', 'X-Title': process.env.SITE_NAME || 'HOC AI' } : {}) };
}
async function fetchJson(url, options) {
  const response = await fetch(url, options);
  const data = await response.json().catch(() => ({}));
  return { response, data };
}
function classify(model) {
  const id = String(model.id || '').toLowerCase();
  const modalities = [...(model.architecture?.input_modalities || []), ...(model.architecture?.output_modalities || [])].map(String);
  const output = String(model.architecture?.output_modalities || '').toLowerCase();
  const image = id.includes('image') || output.includes('image') || modalities.includes('image');
  const audio = id.includes('audio') || id.includes('transcribe') || id.includes('tts') || id.includes('realtime');
  const vision = modalities.includes('image') || /vision|vl|4o|gpt-5|gpt-5\.|gpt-5\.6/i.test(id);
  const chat = !audio && !id.includes('embedding') && !id.includes('moderation') && !image;
  return { chat, vision, image, audio };
}
function normalizeModel(model) {
  return { id: model.id, name: model.name || model.id, description: model.description || '', capabilities: classify(model), contextLength: model.context_length || null, pricing: model.pricing || null, raw: model };
}
export async function discoverApi(api) {
  const provider = api.provider === 'auto' ? (api.key.startsWith('sk-or-') ? 'openrouter' : api.key.startsWith('sk-') ? 'openai' : null) : api.provider;
  if (!provider) return { ...api, provider: null, alive: false, error: 'Cannot infer provider from key.', models: [] };
  const url = provider === 'openai' ? OPENAI_MODELS : OPENROUTER_MODELS;
  try {
    const { response, data } = await fetchJson(url, { headers: headers(api, provider) });
    if (!response.ok) return { ...api, provider, alive: false, status: response.status, error: data?.error?.message || `HTTP ${response.status}`, models: [] };
    return { ...api, provider, alive: true, status: response.status, models: (data.data || []).map(normalizeModel), error: null };
  } catch (error) { return { ...api, provider, alive: false, status: 0, error: error.message, models: [] }; }
}
export function buildCatalog(results) {
  const byCapability = { chat: [], vision: [], image: [], audio: [] };
  for (const result of results) {
    if (!result.alive) continue;
    for (const model of result.models) for (const capability of Object.keys(byCapability)) if (model.capabilities[capability]) byCapability[capability].push({ apiId: result.id, apiName: result.name, provider: result.provider, ...model });
  }
  const score = (item, capability) => {
    let value = item.id.includes(':free') ? 30 : 0;
    if (item.id === 'gpt-5.6-luna' && ['chat', 'vision'].includes(capability)) value += 100;
    if (item.id === 'gpt-image-2' && capability === 'image') value += 120;
    if (item.id === 'gpt-image-1.5' && capability === 'image') value += 80;
    if (item.id.includes('mini')) value += 10;
    return value;
  };
  const selected = {};
  for (const capability of Object.keys(byCapability)) selected[capability] = [...byCapability[capability]].sort((a, b) => score(b, capability) - score(a, capability))[0] || null;
  return { byCapability, selected };
}
export { headers };
