// Pocket Player install page (from Game-Maker: Sandbox Edition's flasher): tabs, and the install buttons.
// The button (ESP Web Tools, vendored in ewt/) is created on the pick, with that board's manifest.
const MANIFESTS = {
  35: { games: 'manifest-35-games.json', courses: 'manifest-35-courses.json', player: 'manifest-35-player.json' },
  28: { games: 'manifest-28-games.json', courses: 'manifest-28-courses.json', player: 'manifest-28-player.json' },
};

function mount(slot, manifest) {
  slot.replaceChildren();
  if (!manifest) return;
  const btn = document.createElement('esp-web-install-button');
  btn.setAttribute('manifest', manifest);
  for (const [name, text] of [
    ['unsupported', "This browser can't flash a board. Use desktop Chrome or Edge."],
    ['not-allowed', 'Open this page from https, not a file.'],
  ]) {
    const s = document.createElement('span');
    s.slot = name;
    s.textContent = text;
    btn.append(s);
  }
  const activate = document.createElement('button');
  activate.type = 'button';
  activate.slot = 'activate';
  activate.className = 'btn';
  activate.textContent = 'Connect and install';
  btn.append(activate);
  slot.append(btn);
}

// The install button shows only with a board picked and the box ticked; the variant (Player + starter games by default) picks the manifest.
const installSlot = document.getElementById('install-slot');
const installAck = document.getElementById('install-ack');
function refreshInstall() {
  const board = document.querySelector('input[name="board"]:checked');
  const variant = document.querySelector('input[name="variant"]:checked');
  const boardNames = { 28: '2.8 inch · ESP32-2432S028', 35: '3.5 inch · ESP32-3248S035' };
  const variantNames = { games: 'Player + starter games', courses: 'Player + Starter courses', player: 'Player only' };
  document.getElementById('install-selection').textContent = board
    ? `${boardNames[board.value]} — ${variantNames[variant.value]}`
    : 'Select your board in step 1 before installing.';
  installSlot.replaceChildren();
  if (board && installAck.checked) {
    mount(installSlot, MANIFESTS[board.value][variant ? variant.value : 'games']);
    return;
  }
  const p = document.createElement('p');
  p.className = 'small';
  p.textContent = board ? 'Check the confirmation box to show the install button.' : 'Pick a board and check the confirmation box to show the install button.';
  installSlot.append(p);
}
for (const r of document.querySelectorAll('input[name="board"], input[name="variant"]')) r.addEventListener('change', refreshInstall);
installAck.addEventListener('change', refreshInstall);
refreshInstall();

const otherSlot = document.getElementById('other-slot');
const ack = document.getElementById('ack');
ack.addEventListener('change', () => {
  if (ack.checked) mount(otherSlot, MANIFESTS[28].games);
  else {
    otherSlot.replaceChildren();
    const p = document.createElement('p');
    p.className = 'small';
    p.textContent = 'Check the confirmation box to show the install button.';
    otherSlot.append(p);
  }
});

// Tabs: arrow keys, Home and End move between them; the hash keeps the tab (#games, #other).
const tabs = [...document.querySelectorAll('[role="tab"]')];
function select(tab, focus) {
  for (const t of tabs) {
    const on = t === tab;
    t.setAttribute('aria-selected', String(on));
    t.tabIndex = on ? 0 : -1;
    document.getElementById(t.getAttribute('aria-controls')).hidden = !on;
  }
  history.replaceState(null, '', '#' + tab.id.replace('tab-', ''));
  if (focus) tab.focus();
}
for (const t of tabs) {
  t.addEventListener('click', () => {
    select(t, false);
  });
  t.addEventListener('keydown', (e) => {
    const i = tabs.indexOf(t);
    const next = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: tabs.length - 1 }[e.key];
    if (next === undefined) return;
    e.preventDefault();
    select(tabs[(next + tabs.length) % tabs.length], true);
  });
}
function selectFromHash(focus = false) {
  const tab = tabs.find((t) => '#' + t.id.replace('tab-', '') === location.hash);
  if (tab) select(tab, focus);
}
window.addEventListener('hashchange', () => selectFromHash(true));
// Follow in-page tab links even if the destination hash is already selected.
for (const link of document.querySelectorAll('a[href^="#"]')) {
  const tab = tabs.find((t) => '#' + t.id.replace('tab-', '') === link.getAttribute('href'));
  if (!tab) continue;
  link.addEventListener('click', (event) => {
    event.preventDefault();
    select(tab, true);
  });
}
selectFromHash();
