# HOC AI multi-key configuration

The web app accepts any number of API keys in JSON. Keys are stored locally in `data/apis.json`, which is gitignored.

Example:

```json
{
  "apis": [
    { "id": "openai-1", "name": "OpenAI 1", "provider": "openai", "key": "sk-...", "enabled": true },
    { "id": "router-1", "name": "OpenRouter 1", "provider": "openrouter", "key": "sk-or-...", "enabled": true }
  ]
}
```

`provider` can also be `auto`; HOC infers `sk-or-` as OpenRouter and `sk-` as OpenAI.

On save, HOC checks every enabled key, discovers its models, classifies capabilities (chat, vision, image, audio), and automatically selects a working model per capability. Dead keys remain visible with their error but are never selected for requests.

The UI never displays the full stored keys after saving; it only shows a masked preview/status.
