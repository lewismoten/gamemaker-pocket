// Board (docs/pocketdisk.js) against a mock Web Serial port that, like Chrome's, refuses close() while a stream is locked.
//   node host/pocketdisktest.mjs
import assert from 'node:assert/strict';
import { Board, fits, roundUp, BLOCK } from '../docs/pocketdisk.js';

function mockPort(answer) {
  const p = { opened: false, closeCalls: 0, rs: null, ws: null };
  p.open = async () => {
    if (p.opened) throw new DOMException('already open', 'InvalidStateError');
    p.opened = true;
    let ctl;  // fresh streams on every open, as a real port gives
    p.rs = new ReadableStream({ start(c) { ctl = c; } });
    p.ws = new WritableStream({
      write(chunk) {
        const r = answer(new TextDecoder().decode(chunk).trim());
        if (r) try { ctl.enqueue(new TextEncoder().encode(r + '\n')); } catch (e) { /* cancelled */ }
      },
    });
  };
  p.setSignals = async () => {};
  p.close = async () => {
    p.closeCalls++;
    if (p.rs.locked || p.ws.locked) throw new TypeError('streams are locked');
    p.opened = false;
  };
  Object.defineProperties(p, { readable: { get: () => p.rs }, writable: { get: () => p.ws } });
  return p;
}
const withPort = (p) => { Object.defineProperty(globalThis, "navigator", { value: { serial: { requestPort: async () => p } }, configurable: true }); };
const hello = (t) => (t === 'DISK HELLO' ? 'DISK HELLO 1 chunk=1024 max=2097152' : '');

// fits does not credit the file it replaces
assert.equal(fits(60000, 60000, 60000 + 2 * BLOCK - 1), false);
assert.equal(fits(60000, 60000, roundUp(60000) + 2 * BLOCK), true);
assert.equal(fits(120000, 60000, 100000), false);

// connect, close, connect again on the same port
{
  const p = mockPort(hello);
  withPort(p);
  const b = new Board();
  await b.connect();
  await b.close();
  assert.equal(p.opened, false, 'port really closed');
  const b2 = new Board();
  await b2.connect();
  await b2.close();
  assert.equal(p.opened, false);
}
// busy answer: error, port left closed
{
  const p = mockPort((t) => (t === 'DISK HELLO' ? 'DISK ERR busy' : ''));
  withPort(p);
  await assert.rejects(new Board().connect(), /not on its game list/);
  assert.equal(p.opened, false, 'busy path closes the port');
}
// no answer (shortened 12 s wait): port left closed
{
  const p = mockPort(() => '');
  withPort(p);
  const realNow = Date.now;
  let t = realNow();
  Date.now = () => (t += 700);
  try { await assert.rejects(new Board().connect(), /didn't answer/); } finally { Date.now = realNow; }
  assert.equal(p.opened, false, 'timeout path closes the port');
}
// a late DISK HELLO is not taken for the answer to a command
{
  const p = mockPort((t) => (t === 'DISK RESCAN' ? 'DISK HELLO 1 chunk=1024 max=2097152\nDISK DONE games=3' : hello(t)));
  withPort(p);
  const b = new Board();
  await b.connect();
  await b.rescan();
  await b.close();
}
// the order list may hold up to 256 names; more is refused before anything is sent
{
  const sent = [];
  const p = mockPort((t) => { sent.push(t); return t === 'DISK ORDER 256' ? 'DISK GO' : /^[A-Z0-9]+\.GMP$/.test(t) ? '' : hello(t); });
  withPort(p);
  const b = new Board();
  await b.connect();
  await assert.rejects(b.setOrder(Array.from({ length: 257 }, (_, i) => `G${i}.GMP`)), /at most 256/);
  assert.ok(!sent.some((t) => t.startsWith('DISK ORDER')));
  await b.close();
}
console.log('pocketdisktest ok');
