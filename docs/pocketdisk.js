// The board's USB file commands (src/serialfs.h) over Web Serial: connect, list, put, delete, order, rescan.
// Opens the port the way scripts/pocketStatus.py does: no reset pulse (both signal lines are released at once after the open),
// and if the board restarts anyway the hello is simply asked again until it answers.
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();
export function crc32(bytes) {
  let c = 0xffffffff;
  for (let i = 0; i < bytes.length; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}
const hex8 = (n) => n.toString(16).padStart(8, '0');
export const BLOCK = 4096;
export const roundUp = (n) => Math.ceil(n / BLOCK) * BLOCK;
// The same rule as the firmware's `fits`: two spare blocks stay free. A file it replaces is NOT credited: the old copy stays
// until the new one is complete and renamed over it, so for a while both are on the board.
export const fits = (size, replaced, free) => size > 0 && roundUp(size) + 2 * BLOCK <= free;
// "My Game 2.gmp" becomes MYGAME2.GMP: upper case, only A-Z 0-9 _ -, at most 8 characters before .GMP; '' if nothing is left.
export function boardName(fileName) {
  const base = fileName.replace(/\.[^.]*$/, '').toUpperCase().replace(/[^A-Z0-9_-]/g, '').slice(0, 8);
  return base ? base + '.GMP' : '';
}

export class Board {
  constructor() { this.port = null; this.lines = []; this.waiters = []; this.chunk = 1024; this.max = 2 * 1024 * 1024; }
  static supported() { return 'serial' in navigator; }
  async connect(onStatus = () => {}) {
    this.port = await navigator.serial.requestPort();
    try {
      await this.port.open({ baudRate: 115200, bufferSize: 8192 });
    } catch (e) { this.port = null; throw e; }
    try {
      try { await this.port.setSignals({ dataTerminalReady: false, requestToSend: false }); } catch (e) { /* not every adapter lets it */ }
      this.writer = this.port.writable.getWriter();
      this.reading = this.pump();
      const end = Date.now() + 12000;
      while (Date.now() < end) {
        onStatus(Date.now() - (end - 12000) > 2500 ? 'Waiting for the board (it may be restarting)...' : 'Asking the board...');
        await this.send('DISK HELLO');
        const l = await this.line((x) => x.startsWith('DISK '), 900).catch(() => null);
        if (!l) continue;
        const m = /^DISK HELLO 1 chunk=(\d+) max=(\d+)/.exec(l);
        if (m) { this.chunk = +m[1]; this.max = +m[2]; return; }
        if (l.startsWith('DISK ERR busy')) throw new Error('The board is not on its game list. Close Settings, setup or the game, then connect again.');
      }
      throw new Error("The board didn't answer. This works only after the firmware is installed (Install tab), and the board must show its game list. Older firmware doesn't have this feature.");
    } catch (e) {
      await this.close();  // every way out leaves the port closed, so Connect works again without a reload
      throw e;
    }
  }
  async pump() {
    const reader = (this.reader = this.port.readable.getReader());
    const dec = new TextDecoder();
    let buf = '';
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        buf += dec.decode(value, { stream: true });
        let i;
        while ((i = buf.indexOf('\n')) >= 0) {
          this.push(buf.slice(0, i).replace(/\r$/, ''));
          buf = buf.slice(i + 1);
        }
        if (buf.length > 4000) buf = '';
      }
    } catch (e) { /* unplugged */ }
    reader.releaseLock();
    this.push(null);
  }
  push(l) {
    this.lines.push(l);
    this.wake();
  }
  wake() {
    for (const w of this.waiters.splice(0)) w();
  }
  // The next line of the board matching test (lines before it are dropped), or a timeout error.
  async line(test, ms = 6000) {
    const end = Date.now() + ms;
    for (;;) {
      while (this.lines.length) {
        const l = this.lines.shift();
        if (l === null) throw new Error('The board was unplugged.');
        if (test(l)) return l;
      }
      const left = end - Date.now();
      if (left <= 0) throw new Error('The board did not answer in time.');
      await new Promise((res) => { this.waiters.push(res); setTimeout(res, left); });
    }
  }
  async send(text) { await this.writer.write(new TextEncoder().encode(text + '\n')); }
  async expect(prefix, ms) {
    const l = await this.line((x) => x.startsWith('DISK ') && !x.startsWith('DISK HELLO'), ms);  // a late answer to a connect retry is not ours
    if (l.startsWith('DISK ERR')) throw new Error('Board: ' + l.slice(9));
    if (!l.startsWith('DISK ' + prefix)) throw new Error('Unexpected answer: ' + l);
    return l;
  }
  // {files: [{name, size}], order: [name], free, total}
  async list() {
    this.lines.length = 0;
    await this.send('DISK LS');
    const r = { files: [], order: [], free: 0, total: 0 };
    for (;;) {
      const l = await this.line((x) => x.startsWith('DISK '), 8000);
      if (l.startsWith('DISK ERR')) throw new Error('Board: ' + l.slice(9));
      const p = l.split(' ');
      if (p[1] === 'F') r.files.push({ name: p[2], size: +p[3] });
      else if (p[1] === 'O') r.order.push(p[2]);
      else if (p[1] === 'FREE') { r.free = +p[2]; r.total = +p[3]; }
      else if (p[1] === 'END') break;
    }
    const names = r.files.map((f) => f.name);
    const first = r.order.filter((n, i) => names.includes(n) && r.order.indexOf(n) === i);
    r.files.sort((a, b) => {
      const ia = first.indexOf(a.name), ib = first.indexOf(b.name);
      return (ia < 0 ? 1e9 : ia) - (ib < 0 ? 1e9 : ib);
    });
    return r;
  }
  // Sends one file in chunks, each with its CRC (a bad one is sent again up to 3 times). onProgress(bytesSent).
  async put(name, bytes, onProgress = () => {}) {
    this.lines.length = 0;
    await this.send(`DISK PUT ${name} ${bytes.length} ${hex8(crc32(bytes))}`);
    await this.expect('GO', 8000);
    for (let off = 0; off < bytes.length; ) {
      const part = bytes.subarray(off, Math.min(off + this.chunk, bytes.length));
      for (let tries = 1; ; tries++) {
        await this.send(`DISK C ${part.length} ${hex8(crc32(part))}`);
        await this.writer.write(part);
        try { await this.expect('OK', 10000); break; } catch (e) {
          if (!/crc/.test(e.message) || tries >= 3) { try { await this.send('DISK ABORT'); } catch (x) { /* gone */ } throw e; }
        }
      }
      off += part.length;
      onProgress(off);
    }
    await this.expect('DONE', 10000);
  }
  async remove(name) { this.lines.length = 0; await this.send(`DISK DEL ${name}`); await this.expect('DONE', 6000); }
  async setOrder(names) {
    this.lines.length = 0;
    await this.send(`DISK ORDER ${names.length}`);
    await this.expect('GO', 6000);
    for (const n of names) await this.send(n);
    await this.expect('DONE', 6000);
  }
  async rescan() { this.lines.length = 0; await this.send('DISK RESCAN'); await this.expect('DONE', 15000); }
  // Cancels the reader and waits for the pump to let go of it, releases the writer, then closes the port (Web Serial refuses to
  // close a port whose streams are still locked).
  async close() {
    const port = this.port;
    this.port = null;
    if (!port) return;
    try { if (this.reader) await this.reader.cancel(); } catch (e) { /* already gone */ }
    try { if (this.reading) await this.reading; } catch (e) { /* ok */ }
    try { if (this.writer) this.writer.releaseLock(); } catch (e) { /* ok */ }
    this.reader = this.writer = this.reading = null;
    try { await port.close(); } catch (e) { /* unplugged */ }
  }
}
