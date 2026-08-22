# HOC AI Image Chat

A small Node/Express chatbot for Codespaces using OpenRouter.

## Features

- Text chat through OpenRouter.
- Upload PNG/JPEG/WebP/GIF images and ask the vision model to describe, OCR, or analyze them.
- Image editing/generation endpoint using OpenRouter's `/api/v1/images` API and reference images.
- Browser UI with image upload and automatic routing between vision and image-edit requests.
- API key stays server-side in `.env` and is ignored by Git.

## Setup

```bash
cd web
npm install
cp .env.example .env
```

Put your OpenRouter key in `.env`:

```env
OPENROUTER_API_KEY=your-key
```

The default vision model is:

```text
nvidia/nemotron-nano-12b-v2-vl:free
```

For image editing, set `OPENROUTER_IMAGE_MODEL` to an image model available to your OpenRouter account that supports image input/reference images and image output. Check the available models with:

```bash
curl -H "Authorization: Bearer $OPENROUTER_API_KEY" http://localhost:3000/api/image-models
```

Then run:

```bash
npm start
```

Open the forwarded port for 3000 in Codespaces.

## API

- `GET /api/health` — configuration status.
- `GET /api/image-models` — image model capability discovery.
- `POST /api/chat` — text chat or image understanding. Use multipart form fields `message` and optional `image`.
- `POST /api/edit-image` — image-to-image editing. Use multipart form fields `message` and `image`.

Image editing is intentionally opt-in through `OPENROUTER_IMAGE_MODEL`; no paid image model is silently selected.
