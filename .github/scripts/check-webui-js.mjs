// check-webui-js.mjs — syntax-check the inline JavaScript embedded in webui.h.
//
// A single parse error in the PROGMEM HTML kills every onclick handler and leaves
// the Status tab stuck on "Loading...". Run this on every PR before firmware build.
import { readFile, writeFile, unlink } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const WEBUI_PATH = 'src/webui.h';
const raw = await readFile(WEBUI_PATH, 'utf8');

const open = raw.indexOf('<script>');
const close = raw.indexOf('</script>', open);
if (open < 0 || close < 0) {
  console.error(`${WEBUI_PATH}: missing <script> block`);
  process.exit(1);
}

const js = raw.slice(open + '<script>'.length, close);
if (!js.trim()) {
  console.error(`${WEBUI_PATH}: empty <script> block`);
  process.exit(1);
}

const tmp = join(tmpdir(), `smalltv-webui-${process.pid}.js`);
await writeFile(tmp, js, 'utf8');

const result = spawnSync(process.execPath, ['--check', tmp], { encoding: 'utf8' });
await unlink(tmp).catch(() => {});

if (result.status !== 0) {
  console.error(`Embedded web UI JavaScript failed syntax check (${WEBUI_PATH}):`);
  if (result.stderr) console.error(result.stderr.trim());
  process.exit(1);
}

console.log(`Embedded web UI JavaScript OK (${js.length} bytes in ${WEBUI_PATH})`);
