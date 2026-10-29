import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';

let action = 0;
let brightness = 80;
const settings = await WebAssembly.instantiate(await readFile(new URL('../wasm_apps/build/settings.wasm', import.meta.url)), {
  env: { host_app_action: () => action, host_app_value: () => brightness },
});
for (brightness of [-100, 0, 20, 40, 80, 100, 400]) {
  for (action of [-1, 0, 1, 7]) {
    const expected = Math.max(20, Math.min(100, brightness + (action === -1 ? -20 : action === 1 ? 20 : 0)));
    assert.equal(settings.instance.exports.run(), expected, `brightness=${brightness} action=${action}`);
  }
}
let now = 0n;
const watch = await WebAssembly.instantiate(await readFile(new URL('../wasm_apps/build/watch.wasm', import.meta.url)), {
  env: { host_time_ms: () => now, host_log_i32: () => {} },
});
for (const [ms, expected] of [[0n, 0], [3723000n, 10203], [86399000n, 235959], [86400000n, 0], [4294967000000n, 62320]]) {
  now = ms;
  assert.equal(watch.instance.exports.run(), expected);
}
console.log('Watch time encoding and Settings brightness bounds passed.');
