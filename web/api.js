import 'dotenv/config';
import express from 'express';
import multer from 'multer';

const app = express();
const port = Number(process.env.PORT || 3000);
const apiKey = process.env.OPENROUTER_API_KEY;
const textModel = process.env.OPENROUTER_TEXT_MODEL || 'openrouter/free';
const visionModel = process.env.OPENROUTER_VISION_MODEL || 'nvidia/nemotron-nano-12b-v2-vl:free';
const imageModel = process.env.OPENROUTER_IMAGE_MODEL || '';

const upload = multer({
  storage: multer.memoryStorage(),
  limits: { fileSize: 10 * 1024 * 1024 },
  fileFilter: (_req, file, cb) => {
    cb(null, /^image\/(png|jpe?g|webp|gif)$/i.test(file.mimetype));
  },
});

app.use(express.json({ limit: '1mb' }));
app.use(express.static('.'));

function requireKey(res) {
  if (!apiKey) {
    res.status(500).json({ error: 'OPENROUTER_API_KEY is not configured.' });
    return false;
  }
  return true;
}

function imageDataUrl(file) {
  return `data:${file.mimetype};base64,${file.buffer.toString('base64')}`;
}

async function openRouterChat(model, messages, extra = {}) {
  const response = await fetch('https://openrouter.ai/api/v1/chat/completions', {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${apiKey}`,
      'Content-Type': 'application/json',
      'HTTP-Referer': process.env.SITE_URL || 'http://localhost:3000',
      'X-Title': process.env.SITE_NAME || 'HOC AI Image Chat',
    },
    body: JSON.stringify({ model, messages, ...extra }),
  });

  const data = await response.json();
  if (!response.ok) {
    const message = data?.error?.message || `OpenRouter request failed (${response.status})`;
    const error = new Error(message);
    error.status = response.status;
    error.details = data;
    throw error;
  }
  return data;
}

function getText(data) {
  return data?.choices?.[0]?.message?.content || '';
}

app.get('/api/health', (_req, res) => {
  res.json({
    ok: true,
    configured: Boolean(apiKey),
    textModel,
    visionModel,
    imageModel: imageModel || null,
  });
});

app.post('/api/chat', upload.single('image'), async (req, res) => {
  if (!requireKey(res)) return;

  const message = String(req.body?.message || '').trim();
  if (!message) return res.status(400).json({ error: 'Message is required.' });

  try {
    const content = [{ type: 'text', text: message }];
    let model = textModel;

    if (req.file) {
      model = visionModel;
      content.push({
        type: 'image_url',
        image_url: { url: imageDataUrl(req.file) },
      });
    }

    const data = await openRouterChat(model, [
      {
        role: 'system',
        content: 'You are a helpful multimodal assistant. If an image is provided, inspect it carefully and answer the user in the same language they use.',
      },
      { role: 'user', content },
    ]);

    res.json({
      type: 'text',
      model,
      answer: getText(data),
    });
  } catch (error) {
    res.status(error.status || 500).json({ error: error.message, details: error.details || null });
  }
});

app.post('/api/edit-image', upload.single('image'), async (req, res) => {
  if (!requireKey(res)) return;
  if (!req.file) return res.status(400).json({ error: 'An image is required.' });
  if (!imageModel) {
    return res.status(501).json({
      error: 'No image generation/editing model is configured.',
      hint: 'Set OPENROUTER_IMAGE_MODEL to an OpenRouter image-capable model available to your account.',
    });
  }

  const instruction = String(req.body?.message || '').trim();
  if (!instruction) return res.status(400).json({ error: 'Edit instruction is required.' });

  try {
    const image = imageDataUrl(req.file);
    const data = await openRouterChat(imageModel, [
      {
        role: 'user',
        content: [
          { type: 'text', text: instruction },
          { type: 'image_url', image_url: { url: image } },
        ],
      },
    ]);

    const message = data?.choices?.[0]?.message;
    const images = message?.images || data?.images || [];
    const first = images[0];
    const imageUrl = first?.image_url?.url || first?.url || null;

    if (!imageUrl) {
      return res.status(502).json({
        error: 'The selected model did not return an edited image.',
        answer: getText(data),
        raw: data,
      });
    }

    res.json({ type: 'image', model: imageModel, image: imageUrl, answer: getText(data) });
  } catch (error) {
    res.status(error.status || 500).json({ error: error.message, details: error.details || null });
  }
});

app.listen(port, () => {
  console.log(`HOC AI Image Chat running at http://localhost:${port}`);
  console.log(`Text model: ${textModel}`);
  console.log(`Vision model: ${visionModel}`);
  console.log(`Image model: ${imageModel || '(not configured)'}`);
});
