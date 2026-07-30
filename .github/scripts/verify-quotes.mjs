// verify-quotes.mjs — sanity-check published quote JSON before pushing to data.
import { readFile, readdir } from 'node:fs/promises';

const cfg = JSON.parse(await readFile('quotes-config.json', 'utf8'));
const keys = cfg.symbols.map((s) => s.key);
const files = new Set(await readdir('out/quotes'));

const missing = keys.filter((k) => !files.has(`${k}.json`));
if (missing.length) {
  console.error(`Missing quote files for keys: ${missing.join(', ')}`);
  process.exit(1);
}

for (const key of keys) {
  const raw = await readFile(`out/quotes/${key}.json`, 'utf8');
  const data = JSON.parse(raw);
  if (!data.ok || data.price == null || !Number.isFinite(data.price)) {
    console.error(`Invalid quote payload for ${key}:`, data);
    process.exit(1);
  }
}

console.log(`Verified ${keys.length} quote file(s) in out/quotes/`);
