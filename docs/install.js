// Pocket Player install page (from Game-Maker: Sandbox Edition's flasher): tabs, and the Install buttons.
// The button (ESP Web Tools, vendored in ewt/) is created on the pick, with that board's manifest.
const MANIFESTS = { 35: 'manifest-35.json', 28: 'manifest-28.json' };

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
  slot.append(btn);
}

const installSlot = document.getElementById('install-slot');
for (const r of document.querySelectorAll('input[name="board"]')) {
  r.addEventListener('change', () => mount(installSlot, MANIFESTS[r.value]));
  if (r.checked) mount(installSlot, MANIFESTS[r.value]);
}

const otherSlot = document.getElementById('other-slot');
const ack = document.getElementById('ack');
ack.addEventListener('change', () => {
  if (ack.checked) mount(otherSlot, MANIFESTS[28]);
  else {
    otherSlot.replaceChildren();
    const p = document.createElement('p');
    p.className = 'small';
    p.textContent = 'Tick the box to show the Install button.';
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
  if (focus) tab.focus();
}
for (const t of tabs) {
  t.addEventListener('click', () => {
    select(t, false);
    history.replaceState(null, '', '#' + t.id.replace('tab-', ''));
  });
  t.addEventListener('keydown', (e) => {
    const i = tabs.indexOf(t);
    const next = { ArrowRight: i + 1, ArrowLeft: i - 1, Home: 0, End: tabs.length - 1 }[e.key];
    if (next === undefined) return;
    e.preventDefault();
    select(tabs[(next + tabs.length) % tabs.length], true);
  });
}
const start = tabs.find((t) => '#' + t.id.replace('tab-', '') === location.hash);
if (start) select(start, false);
