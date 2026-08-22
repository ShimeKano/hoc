import 'dotenv/config';
import express from 'express';
import multer from 'multer';
import { readConfig, writeConfig, publicConfig } from './ai/config.js';
import { discoverApis, chat, generateImage } from './ai/router.js';

const app = express();
const port = Number(process.env.PORT || 3000);
const upload = multer({ storage: multer.memoryStorage(), limits: { fileSize: 12 * 1024 * 1024 }, fileFilter: (_r, f, cb) => cb(null, /^image\/(png|jpe?g|webp|gif)$/i.test(f.mimetype)) });
app.use(express.json({ limit: '1mb' }));
app.use(express.static('.'));

function dataUrl(file) { return `data:${file.mimetype};base64,${file.buffer.toString('base64')}`; }
async function current() { const config = await readConfig(); const { results, catalog } = await discoverApis(config.apis); return { config, results, catalog }; }
function publicResults(results) { return results.map(r => ({ id:r.id,name:r.name,provider:r.provider,alive:r.alive,status:r.status||null,error:r.error||null,modelCount:r.models?.length||0,models:(r.models||[]).map(m=>({id:m.id,name:m.name,capabilities:m.capabilities})) })); }

app.get('/api/status', async (_req,res)=>{try{const x=await current();res.json({apis:publicConfig(x.config).apis,results:publicResults(x.results),selected:x.catalog.selected});}catch(e){res.status(500).json({error:e.message});}});
app.post('/api/apis', async (req,res)=>{try{const config=await writeConfig(req.body);const x=await current();res.json({apis:publicConfig(config).apis,results:publicResults(x.results),selected:x.catalog.selected});}catch(e){res.status(400).json({error:e.message});}});
app.post('/api/refresh', async (_req,res)=>{try{const x=await current();res.json({results:publicResults(x.results),selected:x.catalog.selected});}catch(e){res.status(500).json({error:e.message});}});
app.post('/api/chat', upload.single('image'), async (req,res)=>{try{const message=String(req.body?.message||'').trim();if(!message)return res.status(400).json({error:'Message is required.'});const x=await current();const out=await chat(x.catalog,message,req.file?dataUrl(req.file):null);res.json({type:'text',...out});}catch(e){res.status(e.status||500).json({error:e.message,details:e.details||null});}});
app.post('/api/generate-image', upload.single('image'), async (req,res)=>{try{const prompt=String(req.body?.message||'').trim();if(!prompt)return res.status(400).json({error:'Prompt is required.'});const x=await current();const out=await generateImage(x.catalog,prompt,req.file?dataUrl(req.file):null);res.json({type:'image',...out});}catch(e){res.status(e.status||500).json({error:e.message,details:e.details||null});}});
app.get('/api/health', async (_req,res)=>{try{const x=await current();res.json({ok:true,apis:x.results.length,alive:x.results.filter(r=>r.alive).length,selected:x.catalog.selected});}catch(e){res.status(500).json({ok:false,error:e.message});}});
app.listen(port,()=>console.log(`HOC AI running at http://localhost:${port}`));
