/* StuntGP NEW — ekran wyboru języka w stylu menu gry.
   Koło flag odwzorowuje stronę językową Stunt GP: flagi na elipsie, wybrana flaga najbliżej przycisków.
   Ruch koła jak w grze (strona 452D90): 60 kroków na sekundę, w każdym kroku położenie zbliża się do numeru
   wybranego języka o 10% różnicy + 0,001; lista, tak jak w grze, nie zawija się na końcach.
   Klawisze: strzałki + ENTER. Wybór języka bieli przyciski i wygasza ekran do czerni jak zmiana strony w grze. */
(() => {
  'use strict';

  const stage = document.querySelector('[data-lang-stage]');
  const wheel = document.querySelector('[data-lang-wheel]');
  const list = document.querySelector('[data-lang-list]');
  const dataEl = document.getElementById('lang-data');
  if (!stage || !wheel || !list || !dataEl) return;

  const data = JSON.parse(dataEl.textContent);
  const order = data.order;
  const flags = Array.from(wheel.querySelectorAll('.lang-flag'));
  const buttons = Array.from(list.querySelectorAll('.lang-btn'));
  const hint = document.querySelector('[data-lang-hint]');
  const legal = document.querySelector('[data-lang-legal]');
  const portraitQuery = window.matchMedia('(max-aspect-ratio: 5/6)');
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  const count = order.length;
  const step = (Math.PI * 2) / count;
  const TICK = 1000 / 60;

  const approach = (value, target, minimum) => {
    const diff = target - value;
    const size = Math.abs(diff);
    if (size === 0) return value;
    const move = size * 0.1 + minimum;
    if (!(size > move)) return target;
    return value + (diff > 0 ? move : -move);
  };

  const storage = {
    get() { try { return window.localStorage.getItem('sgn-lang'); } catch { return null; } },
    set(code) { try { window.localStorage.setItem('sgn-lang', code); } catch { /* prywatny tryb */ } },
  };

  const initialCode = () => {
    const saved = storage.get();
    if (saved && order.includes(saved)) return saved;
    const prefs = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language || ''];
    for (const pref of prefs) {
      const code = String(pref).toLowerCase().split('-')[0];
      if (order.includes(code)) return code;
    }
    return order[0];
  };

  let selected = order.indexOf(initialCode());
  let position = selected;   // położenie koła po ostatnim kroku (w numerach flag)
  let previous = selected;   // położenie krok wcześniej, do płynnego rysowania między krokami
  let raf = 0;
  let last = 0;
  let pending = 0;
  let leaving = false;

  // Geometria w jednostkach ekranu 640x480 (landscape) albo 360x640 (portrait).
  const layout = () => {
    if (portraitQuery.matches) {
      return { w: 360, h: 640, place: (t) => ({ x: 180 + 150 * Math.sin(t), y: 38 + 118 * Math.cos(t) }), front: (t) => Math.cos(t) };
    }
    // Parametry elipsy odczytane ze zrzutu strony językowej gry (środek 69,240; półosie 199 i 129,4).
    return { w: 640, h: 480, place: (t) => ({ x: 69 + 199 * Math.cos(t), y: 240 + 129.4 * Math.sin(t) }), front: (t) => Math.cos(t) };
  };

  const draw = (rot) => {
    const box = stage.getBoundingClientRect();
    const L = layout();
    const u = box.width / L.w;
    flags.forEach((flag, i) => {
      const t = (i - rot) * step;
      const p = L.place(t);
      const facing = L.front(t);
      const fw = 106 * u;
      const fh = 64 * u;
      flag.style.width = `${fw}px`;
      flag.style.height = `${fh}px`;
      flag.style.transform = `translate(${(p.x * u - fw / 2).toFixed(2)}px, ${(p.y * u - fh / 2).toFixed(2)}px)`;
      flag.style.opacity = facing < -0.35 ? '0' : '1';
      flag.style.zIndex = String(Math.round((facing + 1) * 10));
    });
  };

  const animate = () => {
    const now = performance.now();
    pending += Math.min(Math.max(0, now - last), 250);
    last = now;
    while (pending >= TICK) {
      previous = position;
      position = approach(position, selected, 0.001);
      pending -= TICK;
    }
    draw(previous + (position - previous) * (pending / TICK));
    if (position === selected && previous === selected) { raf = 0; return; }
    raf = window.requestAnimationFrame(animate);
  };

  const spin = () => {
    if (reduceMotion.matches) {
      position = previous = selected;
      draw(position);
      return;
    }
    if (!raf) {
      last = performance.now();
      pending = 0;
      raf = window.requestAnimationFrame(animate);
    }
  };

  const select = (index, { focus = false } = {}) => {
    if (index < 0 || index >= count) return;
    selected = index;
    buttons.forEach((b, i) => {
      b.classList.toggle('is-selected', i === selected);
      if (i === selected) b.setAttribute('aria-current', 'true'); else b.removeAttribute('aria-current');
    });
    const code = order[selected];
    const texts = data.texts[code];
    if (hint && texts) { hint.textContent = texts.hint; hint.lang = code; }
    if (legal && texts) { legal.textContent = texts.legal; legal.lang = code; }
    if (focus) buttons[selected].focus({ preventScroll: true });
    spin();
  };

  // Zatwierdzenie: przyciski bieleją (odwrotny wjazd, 3 kroki), ekran wygasa w 8 krokach, potem zmiana strony.
  const choose = (index) => {
    if (leaving) return;
    const href = buttons[index].getAttribute('href');
    storage.set(order[index]);
    if (reduceMotion.matches) { window.location.href = href; return; }
    leaving = true;
    list.classList.add('is-leaving');
    document.body.classList.add('is-leaving');
    window.setTimeout(() => { window.location.href = href; }, 9 * TICK);
  };

  buttons.forEach((button, i) => {
    button.addEventListener('mouseenter', () => { if (i !== selected && !leaving) select(i); });
    button.addEventListener('focus', () => { if (i !== selected && !leaving) select(i); });
    button.addEventListener('click', (event) => {
      if (event.button !== 0 || event.metaKey || event.ctrlKey || event.shiftKey || event.altKey) { storage.set(order[i]); return; }
      event.preventDefault();
      choose(i);
    });
  });

  document.addEventListener('keydown', (event) => {
    if (event.altKey || event.ctrlKey || event.metaKey || leaving) return;
    const key = event.key;
    if (key === 'ArrowDown' || key === 'ArrowRight') { event.preventDefault(); select(selected + 1, { focus: true }); }
    else if (key === 'ArrowUp' || key === 'ArrowLeft') { event.preventDefault(); select(selected - 1, { focus: true }); }
    else if (key === 'Home') { event.preventDefault(); select(0, { focus: true }); }
    else if (key === 'End') { event.preventDefault(); select(count - 1, { focus: true }); }
    else if (key === 'Enter' && document.activeElement && !buttons.includes(document.activeElement)) {
      event.preventDefault();
      choose(selected);
    }
  });

  // Powrót przyciskiem „Wstecz” z pamięci podręcznej przeglądarki: ekran znów jasny.
  window.addEventListener('pageshow', (event) => {
    if (!event.persisted) return;
    leaving = false;
    list.classList.remove('is-leaving');
    document.body.classList.remove('is-leaving');
  });

  const relayout = () => draw(position);
  window.addEventListener('resize', relayout);
  if (portraitQuery.addEventListener) portraitQuery.addEventListener('change', relayout);

  select(selected);
  position = previous = selected;
  draw(position);
  buttons[selected].focus({ preventScroll: true });
})();
