// Pocket Player install page (from Game-Maker: Sandbox Edition's flasher): tabs, and the Install buttons.
// The button (ESP Web Tools, vendored in ewt/) is created on the pick, with that board's manifest.
const MANIFESTS = {
  35: { courses: 'manifest-35-courses.json', player: 'manifest-35-player.json' },
  28: { courses: 'manifest-28-courses.json', player: 'manifest-28-player.json' },
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
  slot.append(btn);
}

// The Install button shows only with a board picked and the box ticked; the variant (Player + Starter courses by default) picks the manifest.
const installSlot = document.getElementById('install-slot');
const installAck = document.getElementById('install-ack');
function refreshInstall() {
  const board = document.querySelector('input[name="board"]:checked');
  const variant = document.querySelector('input[name="variant"]:checked');
  installSlot.replaceChildren();
  if (board && installAck.checked) {
    mount(installSlot, MANIFESTS[board.value][variant ? variant.value : 'courses']);
    return;
  }
  const p = document.createElement('p');
  p.className = 'small';
  p.textContent = board ? 'Tick the box to show the Install button.' : 'Pick a board and tick the box to show the Install button.';
  installSlot.append(p);
}
for (const r of document.querySelectorAll('input[name="board"], input[name="variant"]')) r.addEventListener('change', refreshInstall);
installAck.addEventListener('change', refreshInstall);
refreshInstall();

const otherSlot = document.getElementById('other-slot');
const ack = document.getElementById('ack');
ack.addEventListener('change', () => {
  if (ack.checked) mount(otherSlot, MANIFESTS[28].courses);
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
