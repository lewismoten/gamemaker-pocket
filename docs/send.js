// The "Send games" tab: connect over Web Serial, show the board's games and free space, send .GMP files with a progress bar,
// remove and reorder. The protocol is in pocketdisk.js. deliveryHook() below is the place for a delivery animation.
import { Board, boardName, fits, roundUp } from './pocketdisk.js';

const $ = (id) => document.getElementById(id);
const kb = (n) => (n >= 1048576 ? (n / 1048576).toFixed(2) + ' MB' : Math.max(1, Math.round(n / 1024)) + ' KB');

// Called at each step of a delivery: {phase: 'start' | 'progress' | 'done' | 'error', name, sent, total, count, index}.
// `stage` is the empty <div id="delivery-stage"> under the Send button. Does nothing yet; an animation can draw into the stage
// (keep it decorative: the progress bar and the status line already say everything).
export function deliveryHook(event, stage) {
  stage.dispatchEvent(new CustomEvent('delivery', { detail: event }));
}

let board = null;
let state = { files: [], free: 0, total: 0 };
let picked = [];  // {file, name, size, note}
let busy = false;

function status(text, kind = '') {
  const s = $('send-status');
  s.textContent = text;
  s.className = 'small st ' + kind;
}
function setBusy(b) {
  busy = b;
  for (const el of document.querySelectorAll('#send-board button, #send-board input')) el.disabled = b;
  if (!b) refreshPicked();
}

function renderBoard() {
  const used = state.total - state.free;
  $('send-space').textContent = `${kb(state.free)} free of ${kb(state.total)} (${state.files.length} game${state.files.length === 1 ? '' : 's'} on the board)`;
  const m = $('send-meter');
  m.max = state.total;
  m.value = used;
  const list = $('send-list');
  list.replaceChildren();
  if (!state.files.length) {
    const li = document.createElement('li');
    li.textContent = 'No games on the board yet.';
    list.append(li);
  }
  state.files.forEach((f, i) => {
    const li = document.createElement('li');
    li.className = 'gamerow';
    const label = document.createElement('span');
    label.className = 'gname';
    label.textContent = `${f.name} (${kb(f.size)})`;
    li.append(label);
    const mk = (text, aria, fn, off) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.className = 'btn small-btn';
      b.textContent = text;
      b.setAttribute('aria-label', aria + ' ' + f.name);
      b.disabled = off || busy;
      b.addEventListener('click', () => fn(b));
      li.append(b);
    };
    mk('Up', 'Move up', () => move(i, -1), i === 0);
    mk('Down', 'Move down', () => move(i, 1), i === state.files.length - 1);
    mk('Remove', 'Remove', (b) => {
      if (b.dataset.sure) return remove(f.name);
      b.dataset.sure = '1';
      b.textContent = 'Really remove?';
      setTimeout(() => { if (b.isConnected) { delete b.dataset.sure; b.textContent = 'Remove'; } }, 4000);
    });
    list.append(li);
  });
  refreshPicked();
}

async function reload() {
  state = await board.list();
  renderBoard();
}
async function guarded(label, fn) {
  setBusy(true);
  try {
    await fn();
    status(label, 'ok');
  } catch (e) {
    status(e.message, 'no');
    try { await reload(); } catch (x) { /* keep the error */ }
  }
  setBusy(false);
}
async function move(i, d) {
  await guarded('Order saved.', async () => {
    const names = state.files.map((f) => f.name);
    [names[i], names[i + d]] = [names[i + d], names[i]];
    await board.setOrder(names);
    await board.rescan();
    await reload();
  });
}
async function remove(name) {
  await guarded(`Removed ${name}.`, async () => {
    await board.remove(name);
    await board.setOrder(state.files.map((f) => f.name).filter((n) => n !== name));
    await board.rescan();
    await reload();
  });
}

// What the user picked, and whether it all fits (in the order shown, replacing a same-named game).
function refreshPicked() {
  const ul = $('send-picked');
  ul.replaceChildren();
  let free = state.free;
  const seen = new Set();
  for (const p of picked) {
    const old = state.files.find((f) => f.name === p.name);
    let bad = '';
    if (!p.name) bad = "can't be sent: its name has no letters or digits";
    else if (p.size > board.max) bad = `too big (the limit is ${kb(board.max)} per game)`;
    else if (seen.has(p.name)) bad = 'has the same board name as another picked file';
    else if (!fits(p.size, old ? old.size : 0, free)) bad = 'does not fit in the space left';
    p.good = !bad;
    if (p.good) free -= roundUp(p.size) - (old ? roundUp(old.size) : 0);
    seen.add(p.name);
    const li = document.createElement('li');
    const msg = bad || (old ? 'replaces the copy on the board' : 'fits') + (p.note ? '; ' + p.note : '');
    li.textContent = `${p.file.name} as ${p.name || '?'} (${kb(p.size)}): ${msg}`;
    li.className = p.good ? 'okline' : 'noline';
    ul.append(li);
  }
  $('send-go').disabled = busy || !picked.length || picked.some((p) => !p.good);
}

function pick(files) {
  picked = [...files].map((file) => {
    const name = boardName(file.name);
    const exact = /^[A-Z0-9_-]{1,8}\.GMP$/.test(file.name);
    return { file, name, size: file.size, note: exact ? '' : `saved as ${name}`, good: false };
  });
  refreshPicked();
}

async function sendAll() {
  const stage = $('delivery-stage');
  const prog = $('send-progress');
  const todo = picked.filter((p) => p.good);
  prog.hidden = false;
  setBusy(true);
  try {
    for (let i = 0; i < todo.length; i++) {
      const p = todo[i];
      const bytes = new Uint8Array(await p.file.arrayBuffer());
      prog.max = bytes.length;
      prog.value = 0;
      prog.setAttribute('aria-label', `Sending ${p.name}`);
      const ev = { name: p.name, total: bytes.length, count: todo.length, index: i };
      deliveryHook({ phase: 'start', sent: 0, ...ev }, stage);
      status(`Sending ${p.name} (${i + 1} of ${todo.length})...`);
      await board.put(p.name, bytes, (sent) => {
        prog.value = sent;
        deliveryHook({ phase: 'progress', sent, ...ev }, stage);
      });
      deliveryHook({ phase: 'done', sent: bytes.length, ...ev }, stage);
    }
    // New games go to the end of the order, after the ones already there.
    await reload();
    await board.setOrder(state.files.map((f) => f.name));
    await board.rescan();
    await reload();
    picked = [];
    $('send-files').value = '';
    status(`Sent ${todo.length} game${todo.length === 1 ? '' : 's'}. The board's list has them now.`, 'ok');
  } catch (e) {
    deliveryHook({ phase: 'error', name: '', sent: 0, total: 0, count: 0, index: 0 }, stage);
    status(e.message + ' Nothing half-written is kept; the games already on the board are unchanged.', 'no');
    try { await reload(); } catch (x) { /* keep the error */ }
  }
  prog.hidden = true;
  setBusy(false);
}

async function connect() {
  $('send-connect').disabled = true;
  try {
    board = new Board();
    await board.connect((t) => status(t));
    await reload();
    $('send-board').hidden = false;
    $('send-connect').hidden = true;
    $('send-disconnect').hidden = false;
    status('Connected.', 'ok');
  } catch (e) {
    board = null;
    if (e && e.name === 'NotFoundError') status('No port was chosen.');
    else status(e.message || String(e), 'no');
  }
  $('send-connect').disabled = false;
}
async function disconnect() {
  if (board) await board.close();
  board = null;
  picked = [];
  $('send-board').hidden = true;
  $('send-connect').hidden = false;
  $('send-disconnect').hidden = true;
  status('Disconnected.');
}

if (!Board.supported()) {
  $('send-connect').disabled = true;
  status("This browser can't talk to the board over USB. Use desktop Chrome or Edge.", 'no');
} else {
  $('send-connect').addEventListener('click', connect);
  $('send-disconnect').addEventListener('click', disconnect);
  $('send-files').addEventListener('change', (e) => pick(e.target.files));
  $('send-go').addEventListener('click', sendAll);
}
