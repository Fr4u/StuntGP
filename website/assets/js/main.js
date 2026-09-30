/* StuntGP NEW — interakcje strony. Bez zależności. Teksty zależne od języka pochodzą z #i18n. */
(() => {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const reduceMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  const i18nEl = document.getElementById('i18n');
  const T = i18nEl ? JSON.parse(i18nEl.textContent) : {};
  const fill = (pattern, values) => String(pattern || '').replace(/\{(\w+)\}/g, (m, k) => (k in values ? values[k] : m));
  const isPlainClick = (event) => event.button === 0 && !event.metaKey && !event.ctrlKey && !event.shiftKey && !event.altKey;

  /* ---------- czas gry ----------
     Interfejs Stunt GP liczy 60 kroków na sekundę. Wartość wejścia logo, przycisków i ikon maleje od 1 do 0
     o 10% różnicy + 0,001 na krok (44 kroki); enterValue() to postać zamknięta tego ciągu. */
  const TICK = 1000 / 60;
  const ENTER_TICKS = 44;
  const moving = () => !reduceMotion.matches;
  const enterValue = (ticks) => Math.max(0, 1.01 * Math.pow(0.9, ticks) - 0.01);
  const hasIO = 'IntersectionObserver' in window;
  const onFirstView = (el, run, rootMargin = '0px 0px -12% 0px') => {
    if (!hasIO) { run(); return; }
    const io = new IntersectionObserver((entries) => {
      if (!entries.some((entry) => entry.isIntersecting)) return;
      io.disconnect();
      run();
    }, { rootMargin });
    io.observe(el);
  };
  const whileVisible = (el, onChange) => {
    if (!hasIO) { onChange(true); return; }
    new IntersectionObserver((entries) => onChange(entries[entries.length - 1].isIntersecting)).observe(el);
  };

  /* ---------- zapamiętany język ---------- */
  const storeLang = (code) => { try { window.localStorage.setItem('sgn-lang', code); } catch { /* tryb prywatny */ } };
  if (T.lang) storeLang(T.lang);

  /* ---------- zmiana strony: wygaszenie do czerni jak w grze (8 kroków + krok przerwy) ---------- */
  const leaveTo = (url) => {
    if (!moving()) { window.location.href = url; return; }
    document.body.classList.add('is-leaving');
    window.setTimeout(() => { window.location.href = url; }, 9 * TICK);
  };
  window.addEventListener('pageshow', (event) => { if (event.persisted) document.body.classList.remove('is-leaving'); });

  /* ---------- górny pasek i menu mobilne ---------- */
  const topbar = $('[data-topbar]');
  const onScroll = () => topbar && topbar.classList.toggle('is-scrolled', window.scrollY > 8);
  window.addEventListener('scroll', onScroll, { passive: true });
  onScroll();

  const toggle = $('[data-nav-toggle]');
  const nav = $('[data-nav]');
  const setNavOpen = (open) => {
    if (!toggle || !nav) return;
    toggle.setAttribute('aria-expanded', String(open));
    nav.classList.toggle('is-open', open);
  };
  if (toggle && nav) {
    toggle.addEventListener('click', () => { setLangOpen(false); setNavOpen(toggle.getAttribute('aria-expanded') !== 'true'); });
    nav.addEventListener('click', (event) => { if (event.target.closest('a')) setNavOpen(false); });
    const wide = window.matchMedia('(min-width: 1321px)');
    const closeOnWide = () => { if (wide.matches) setNavOpen(false); };
    if (wide.addEventListener) wide.addEventListener('change', closeOnWide);
  }

  /* ---------- przełącznik języka ---------- */
  const langBtn = $('[data-langsw-btn]');
  const langMenu = $('[data-langsw-menu]');
  function setLangOpen(open) {
    if (!langBtn || !langMenu) return;
    langBtn.setAttribute('aria-expanded', String(open));
    langMenu.hidden = !open;
  }
  if (langBtn && langMenu) {
    langBtn.addEventListener('click', () => {
      const open = langBtn.getAttribute('aria-expanded') !== 'true';
      setNavOpen(false);
      setLangOpen(open);
      if (open) { const current = $('a[aria-current="page"]', langMenu) || $('a', langMenu); if (current) current.focus(); }
    });
    langMenu.addEventListener('click', (event) => {
      const link = event.target.closest('a');
      if (!link) return;
      if (link.dataset.lang) {
        storeLang(link.dataset.lang);
        if (window.location.hash) link.setAttribute('href', link.getAttribute('href').split('#')[0] + window.location.hash);
      }
      if (!isPlainClick(event) || link.target) return;
      event.preventDefault();
      leaveTo(link.href);
    });
    document.addEventListener('click', (event) => {
      if (!langMenu.hidden && !event.target.closest('[data-langsw]')) setLangOpen(false);
    });
    langMenu.addEventListener('keydown', (event) => {
      const links = $$('a', langMenu);
      const i = links.indexOf(document.activeElement);
      if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault();
        const step = event.key === 'ArrowDown' ? 1 : -1;
        links[(i + step + links.length) % links.length].focus();
      }
    });
  }
  document.addEventListener('keydown', (event) => {
    if (event.key !== 'Escape') return;
    if (langMenu && !langMenu.hidden) { setLangOpen(false); langBtn.focus(); }
    else if (nav && nav.classList.contains('is-open')) { setNavOpen(false); toggle.focus(); }
  });

  /* ---------- podświetlenie aktywnej sekcji ---------- */
  const navLinks = $$('.nav__link');
  const parentOf = { features: 'game' };
  if (hasIO && navLinks.length) {
    const setActive = (id) => {
      const target = parentOf[id] || id;
      navLinks.forEach((link) => link.classList.toggle('is-active', link.getAttribute('href') === '#' + target));
    };
    const observer = new IntersectionObserver((entries) => {
      entries.forEach((entry) => { if (entry.isIntersecting) setActive(entry.target.id); });
    }, { rootMargin: '-45% 0px -50% 0px' });
    $$('main > section[id]').forEach((section) => observer.observe(section));
  }

  /* ---------- przewijane tło: warstwy poza ekranem stoją ---------- */
  if (hasIO) {
    const tiles = new IntersectionObserver((entries) => {
      entries.forEach((entry) => entry.target.classList.toggle('is-offscreen', !entry.isIntersecting));
    });
    $$('.tile').forEach((tile) => tiles.observe(tile));
  }

  /* ---------- ekran tytułowy ----------
     Logo spada z góry (CSS). Po 600 krokach (10 s) na ekranie tytułowym zaczyna podskakiwać:
     y = −30 × sin(π × ułamek fazy), faza rośnie o 0,03 na krok (podskok co 0,555 s; logo ma 334 jednostki wysokości).
     W co siódmym podskoku z ośmiu przez logo przechodzi pasek połysku (ponownie narysowana warstwa addytywna
     w pasie 16 jednostek, który przesuwa się z góry na dół ekranu 640×480), w ósmym logo przewija się w lewo
     o szerokość ekranu i wraca z prawej. Napis z prośbą o ENTER zmienia kolor z białego na 0xFF6060 co podskok. */
  const hero = $('#start');
  const hop = $('[data-title-logo]');
  const prompt = $('[data-title-prompt]');
  const titleBorn = performance.now();
  if (hero && hop && moving()) {
    const slide = $('.hero__logo-slide', hop);
    const logo = $('.hero__logo', slide);
    const wrap = slide.cloneNode(true);
    wrap.classList.add('hero__logo-slide--copy');
    $$('img', wrap).forEach((img) => { img.alt = ''; img.removeAttribute('fetchpriority'); });
    const shine = logo.cloneNode(false);
    shine.className = 'hero__logo hero__logo--shine';
    shine.alt = '';
    shine.removeAttribute('fetchpriority');
    [wrap, shine].forEach((el) => { el.setAttribute('aria-hidden', 'true'); el.hidden = true; hop.appendChild(el); });
    const reset = () => {
      hop.style.transform = '';
      slide.style.transform = '';
      wrap.hidden = true;
      shine.hidden = true;
      if (prompt) prompt.classList.remove('is-alt');
    };
    let raf = 0;
    let shown = false;
    const frame = () => {
      raf = 0;
      if (!moving()) { reset(); return; }
      const phase = Math.max(0, (performance.now() - titleBorn) / TICK - 600) * 0.03;
      const whole = Math.floor(phase);
      const frac = phase - whole;
      const lift = Math.sin(Math.PI * frac);
      hop.style.transform = `translateY(${(-lift * 30 / 334 * 100).toFixed(3)}%)`;
      const cycle = whole % 8;
      if (cycle === 7) {
        const width = hero.clientWidth;
        slide.style.transform = `translateX(${(-width * frac).toFixed(1)}px)`;
        wrap.style.transform = `translateX(${(width - width * frac).toFixed(1)}px)`;
        wrap.hidden = false;
      } else if (!wrap.hidden) {
        slide.style.transform = '';
        wrap.hidden = true;
      }
      if (cycle === 6) {
        const top = (frac * 480 - 84 + 30 * lift) / 334 * 100;
        const band = 16 / 334 * 100;
        const outside = top >= 100 || top + band <= 0;
        if (!outside) shine.style.clipPath = `inset(${Math.max(0, top).toFixed(2)}% 0 ${Math.max(0, 100 - top - band).toFixed(2)}% 0)`;
        shine.hidden = outside;
      } else if (!shine.hidden) {
        shine.hidden = true;
      }
      if (prompt) prompt.classList.toggle('is-alt', whole % 2 === 1);
      if (shown) raf = window.requestAnimationFrame(frame);
    };
    window.setTimeout(() => {
      whileVisible(hero, (visible) => {
        shown = visible;
        if (visible && !raf) raf = window.requestAnimationFrame(frame);
      });
    }, Math.max(0, 600 * TICK - (performance.now() - titleBorn)));
  }
  if (hero && prompt) {
    // „Naciśnij ENTER”: ENTER bez aktywnego elementu przechodzi dalej, gdy ekran tytułowy jest widoczny.
    document.addEventListener('keydown', (event) => {
      if (event.key !== 'Enter' || event.defaultPrevented || event.repeat || event.isComposing) return;
      if (event.altKey || event.ctrlKey || event.metaKey || event.shiftKey) return;
      const active = document.activeElement;
      if (active && active !== document.body && active !== document.documentElement) return;
      const box = hero.getBoundingClientRect();
      if (box.bottom <= 0 || box.top >= window.innerHeight) return;
      event.preventDefault();
      prompt.click();
    });
  }

  /* ---------- wjazd wierszy menu ----------
     Jak MenuText w grze: wiersze wjeżdżają z prawej o szerokość ekranu, dolny rusza pierwszy, każdy wyższy
     o krok później; nad każdym leży biała warstwa, która znika pod koniec wjazdu (CSS: .fly-*). */
  const flyIn = (root, rowSelector, coverSelector, onStart) => {
    if (!root) return;
    const rows = $$(rowSelector, root);
    if (!rows.length || !moving()) { if (onStart) onFirstView(root, onStart); return; }
    rows.forEach((row, i) => {
      row.classList.add('fly-row');
      row.style.setProperty('--fly-i', String(rows.length - 1 - i));
      const flash = document.createElement('span');
      flash.className = 'fly-flash';
      flash.setAttribute('aria-hidden', 'true');
      (coverSelector ? $(coverSelector, row) : row).appendChild(flash);
    });
    root.classList.add('fly-wait');
    onFirstView(root, () => {
      root.classList.replace('fly-wait', 'fly-go');
      if (onStart) onStart();
      window.setTimeout(() => {
        root.classList.remove('fly-go');
        rows.forEach((row) => { row.classList.remove('fly-row'); row.style.removeProperty('--fly-i'); });
        $$('.fly-flash', root).forEach((flash) => flash.remove());
      }, (ENTER_TICKS + rows.length + 2) * TICK);
    });
  };

  /* ---------- pokaz slajdów w hero ---------- */
  const slideshow = $('[data-slideshow]');
  if (slideshow) {
    const slides = $$('.slideshow__slide', slideshow);
    const caption = $('[data-slide-caption]');
    const dotsWrap = $('[data-slide-dots]');
    const hoverArea = slideshow.closest('.hero__visual') || slideshow;
    let index = 0;
    let timer = null;
    let dots = [];

    const go = (next) => {
      // Poprzedni slajd dogasa z niezmienionym najazdem kamery (klasa is-prev ma tę samą animację).
      slides.forEach((slide) => slide.classList.remove('is-prev'));
      if (next !== index) slides[index].classList.add('is-prev');
      slides[index].classList.remove('is-active');
      index = (next + slides.length) % slides.length;
      const slide = slides[index];
      const img = $('img', slide);
      if (img && img.loading === 'lazy') img.loading = 'eager';
      slide.classList.add('is-active');
      if (caption) caption.textContent = slide.dataset.caption || '';
      dots.forEach((dot, i) => dot.setAttribute('aria-current', i === index ? 'true' : 'false'));
    };
    const stop = () => { if (timer) window.clearInterval(timer); timer = null; };
    const start = () => {
      stop();
      if (reduceMotion.matches || slides.length < 2 || document.hidden) return;
      timer = window.setInterval(() => go(index + 1), 5500);
    };

    dots = slides.map((slide, i) => {
      const dot = document.createElement('button');
      dot.type = 'button';
      dot.className = 'slideshow__dot';
      dot.setAttribute('aria-label', fill(T.slideDot, { n: i + 1, total: slides.length, caption: slide.dataset.caption || '' }));
      dot.addEventListener('click', () => { go(i); start(); });
      if (dotsWrap) dotsWrap.appendChild(dot);
      return dot;
    });

    hoverArea.addEventListener('mouseenter', stop);
    hoverArea.addEventListener('mouseleave', start);
    hoverArea.addEventListener('focusin', stop);
    hoverArea.addEventListener('focusout', start);
    document.addEventListener('visibilitychange', () => (document.hidden ? stop() : start()));
    if (reduceMotion.addEventListener) reduceMotion.addEventListener('change', start);
    window.addEventListener('load', () => {
      slides.forEach((slide) => { const img = $('img', slide); if (img) img.loading = 'eager'; });
    });

    go(0);
    start();
  }

  /* ---------- replika menu SINGLE PLAYER (teksty i ikony z plików gry) ----------
     Ikona wybranego trybu: x = 150 − 320 × wjazd, y = 260 − 40 × sin(π × ułamek fazy) w jednostkach ekranu 640×480,
     ikona ma 126×126 jednostek. Przy każdej zmianie wyboru wjazd wraca do 1, a faza do 0 (rośnie o 0,015 na krok). */
  const modes = $('[data-modes]');
  if (modes) {
    const buttons = $$('.menu-btn', modes);
    const outMain = $('[data-help-main-out]');
    const outSub = $('[data-help-sub-out]');
    const iconBox = $('[data-mode-icon]');
    const icons = iconBox ? $$('.mode-icon__img', iconBox) : [];
    let current = Math.max(0, buttons.findIndex((b) => b.classList.contains('is-selected')));
    let pickedAt = performance.now();
    let iconRaf = 0;
    let iconShown = false;
    let entered = !moving();

    const drawIcon = (now) => {
      const img = icons[current];
      if (!img) return;
      if (!moving()) { img.style.transform = ''; return; }
      if (!entered) { img.style.transform = 'translate(-254%, -50%)'; return; }
      const ticks = Math.max(0, (now - pickedAt) / TICK);
      const phase = ticks * 0.015;
      const lift = Math.sin(Math.PI * (phase - Math.floor(phase)));
      const x = -enterValue(ticks) * 320 / 126 * 100;
      const y = -50 - lift * 40 / 126 * 100;
      img.style.transform = `translate(${x.toFixed(2)}%, ${y.toFixed(2)}%)`;
    };
    const iconLoop = () => {
      iconRaf = 0;
      drawIcon(performance.now());
      if (iconShown && moving()) iconRaf = window.requestAnimationFrame(iconLoop);
    };
    const select = (button) => {
      const index = buttons.indexOf(button);
      buttons.forEach((b) => b.classList.toggle('is-selected', b === button));
      if (outMain) outMain.textContent = button.dataset.helpMain || '';
      if (outSub) outSub.textContent = button.dataset.helpSub ? `(${button.dataset.helpSub})` : '';
      if (index === current) return;
      icons.forEach((img, i) => {
        img.classList.toggle('is-active', i === index);
        if (i !== index) img.style.transform = '';
      });
      current = index;
      pickedAt = performance.now();
      drawIcon(pickedAt);
    };
    buttons.forEach((button) => {
      let pressed = 0;
      button.addEventListener('mouseenter', () => select(button));
      button.addEventListener('focus', () => select(button));
      button.addEventListener('click', () => {
        select(button);
        // Zatwierdzony wiersz ma w grze przez 6 kroków czerwony tekst.
        button.classList.add('is-pressed');
        window.clearTimeout(pressed);
        pressed = window.setTimeout(() => button.classList.remove('is-pressed'), 6 * TICK);
      });
    });
    modes.addEventListener('keydown', (event) => {
      const at = buttons.indexOf(document.activeElement);
      if (at < 0 || (event.key !== 'ArrowDown' && event.key !== 'ArrowUp')) return;
      event.preventDefault();
      // Jak w grze: lista nie zawija się na końcach.
      const next = at + (event.key === 'ArrowDown' ? 1 : -1);
      if (next >= 0 && next < buttons.length) buttons[next].focus();
    });

    if (icons.length) drawIcon(performance.now());
    flyIn(modes, '.menu-btn', null, () => {
      entered = true;
      pickedAt = performance.now();
      if (!icons.length || !moving()) return;
      whileVisible(iconBox, (visible) => {
        iconShown = visible;
        if (visible && !iconRaf) iconRaf = window.requestAnimationFrame(iconLoop);
      });
    });
    if (reduceMotion.addEventListener) reduceMotion.addEventListener('change', () => drawIcon(performance.now()));
  }

  /* ---------- galeria: filtr ---------- */
  const gallery = $('[data-gallery]');
  const items = gallery ? $$('.gallery__item', gallery) : [];
  const filters = $('[data-filters]');
  if (filters) {
    const filterButtons = $$('button', filters);
    filterButtons.forEach((button) => {
      button.addEventListener('click', () => {
        const wanted = button.dataset.filter;
        filterButtons.forEach((b) => {
          const on = b === button;
          b.classList.toggle('is-selected', on);
          b.setAttribute('aria-pressed', String(on));
        });
        items.forEach((item) => {
          const cats = (item.dataset.cat || '').split(' ');
          item.hidden = !(wanted === 'all' || cats.includes(wanted));
        });
      });
    });
  }
  flyIn(filters, 'button', null);
  flyIn($('.faq'), '.faq__item', 'summary');

  /* ---------- panele otwierane jak w grze: z kwadratu 48×48 jednostek na środku ---------- */
  const openPanel = (panel) => {
    if (!panel || !moving()) return;
    panel.classList.add('panel-wait');
    onFirstView(panel, () => {
      panel.classList.replace('panel-wait', 'panel-go');
      window.setTimeout(() => panel.classList.remove('panel-go'), 31 * TICK);
    });
  };
  openPanel($('.garage__frame'));
  openPanel($('.download__card'));

  /* ---------- odliczanie 3-2-1-GO: każdy znak po sekundzie, jak przy starcie wyścigu ---------- */
  const countdown = $('.countdown');
  if (countdown && moving()) {
    const steps = $$('.countdown__step', countdown);
    countdown.classList.add('count-wait');
    onFirstView(countdown, () => {
      steps.forEach((step, i) => window.setTimeout(() => step.classList.add('is-lit'), i * 60 * TICK));
      window.setTimeout(() => {
        countdown.classList.remove('count-wait');
        steps.forEach((step) => step.classList.remove('is-lit'));
      }, steps.length * 60 * TICK);
    }, '0px 0px -25% 0px');
  }

  /* ---------- oprawa strony ---------- */
  // Pasek postępu przewijania pod górnym paskiem.
  if (topbar) {
    let queued = false;
    const progress = () => {
      queued = false;
      const max = document.documentElement.scrollHeight - window.innerHeight;
      topbar.style.setProperty('--scroll', max > 0 ? Math.min(1, Math.max(0, window.scrollY / max)).toFixed(4) : '0');
    };
    window.addEventListener('scroll', () => { if (!queued) { queued = true; window.requestAnimationFrame(progress); } }, { passive: true });
    window.addEventListener('resize', progress);
    progress();
  }

  if (moving() && hasIO) {
    // Paski tytułowe otwierają się od środka, tytuły sekcji wysuwają się pod nimi.
    $$('.section__head .titlebar, .menu-replica > .titlebar').forEach((bar) => {
      bar.classList.add('tb-wait');
      onFirstView(bar, () => {
        bar.classList.replace('tb-wait', 'tb-go');
        window.setTimeout(() => bar.classList.remove('tb-go'), 31 * TICK);
      }, '0px 0px -6% 0px');
    });
    $$('.section__title').forEach((title) => {
      title.classList.add('tt-wait');
      onFirstView(title, () => {
        title.classList.replace('tt-wait', 'tt-go');
        window.setTimeout(() => title.classList.remove('tt-go'), 1200);
      }, '0px 0px -6% 0px');
    });

    // Karty i panele wchodzą kaskadowo: w każdej porcji, która pojawia się na ekranie, od lewej do prawej i z góry na dół.
    const reveal = new IntersectionObserver((entries) => {
      const coming = entries.filter((entry) => entry.isIntersecting).map((entry) => entry.target);
      coming.sort((a, b) => {
        const ra = a.getBoundingClientRect();
        const rb = b.getBoundingClientRect();
        return Math.round(ra.top - rb.top) || ra.left - rb.left;
      });
      coming.forEach((el, i) => {
        reveal.unobserve(el);
        const delay = Math.min(i, 7) * 70;
        el.style.setProperty('--rv-d', `${delay}ms`);
        el.classList.add('rv-in');
        window.setTimeout(() => { el.classList.remove('rv', 'rv-in'); el.style.removeProperty('--rv-d'); }, delay + 900);
      });
    }, { rootMargin: '0px 0px -6% 0px' });
    $$('.hud-stat, .about__text, .feature, .gallery__item, .team-btn, .garage__info, .download__side > *, .controls > *, .status > *, .timeline__item, .method__grid > *, .finish__inner > :not(.finish__sign)')
      .forEach((el) => { el.classList.add('rv'); reveal.observe(el); });

    // Flaga FINISH spada na metę.
    const flag = $('.finish__sign');
    if (flag) {
      flag.classList.add('fin-wait');
      onFirstView(flag, () => flag.classList.replace('fin-wait', 'fin-go'), '0px 0px -10% 0px');
    }

    // Liczniki HUD odliczają do swoich wartości tą samą krzywą co wjazd menu, tylko wolniej.
    const hud = $('.hud-stats');
    if (hud) {
      const values = $$('.hud-stat__value', hud).map((el) => ({ el, text: el.textContent })).filter((v) => /\d/.test(v.text));
      values.forEach(({ el, text }) => { el.textContent = text.replace(/\d+/g, '0'); });
      onFirstView(hud, () => {
        const start = performance.now();
        const count = () => {
          const v = enterValue((performance.now() - start) / (TICK * 2));
          values.forEach(({ el, text }) => { el.textContent = v > 0 ? text.replace(/\d+/g, (n) => String(Math.round(Number(n) * (1 - v)))) : text; });
          if (v > 0) window.requestAnimationFrame(count);
        };
        window.requestAnimationFrame(count);
      }, '0px 0px -4% 0px');
    }
  }

  // Głębia nagłówka: logo i zrzut przechylają się za kursorem (tylko mysz, płynne wygładzanie).
  if (hero && moving() && window.matchMedia('(hover: hover) and (pointer: fine)').matches) {
    let tx = 0;
    let ty = 0;
    let x = 0;
    let y = 0;
    let raf = 0;
    const ease = () => {
      raf = 0;
      x += (tx - x) * 0.12;
      y += (ty - y) * 0.12;
      hero.style.setProperty('--px', x.toFixed(4));
      hero.style.setProperty('--py', y.toFixed(4));
      if (Math.abs(tx - x) > 0.001 || Math.abs(ty - y) > 0.001) raf = window.requestAnimationFrame(ease);
    };
    hero.addEventListener('pointermove', (event) => {
      const box = hero.getBoundingClientRect();
      tx = Math.max(-1, Math.min(1, ((event.clientX - box.left) / box.width) * 2 - 1));
      ty = Math.max(-1, Math.min(1, ((event.clientY - box.top) / box.height) * 2 - 1));
      if (!raf) raf = window.requestAnimationFrame(ease);
    });
    hero.addEventListener('pointerleave', () => {
      tx = 0;
      ty = 0;
      if (!raf) raf = window.requestAnimationFrame(ease);
    });
  }

  /* ---------- podgląd pełnego zrzutu: wspólny dla galerii i wyboru pojazdu ---------- */
  const dialog = $('[data-lightbox]');
  const canLightbox = Boolean(dialog && typeof dialog.showModal === 'function');
  let openLightbox = null;

  if (canLightbox) {
    const img = $('[data-lightbox-img]', dialog);
    const caption = $('[data-lightbox-caption]', dialog);
    const count = $('[data-lightbox-count]', dialog);
    let list = [];
    let index = 0;
    let opener = null;

    const show = (next) => {
      if (!list.length) return;
      index = (next + list.length) % list.length;
      const entry = list[index];
      img.src = entry.src;
      img.alt = entry.alt;
      caption.textContent = entry.caption;
      count.textContent = `${index + 1} / ${list.length}`;
      const after = list[(index + 1) % list.length];
      if (after) { const pre = new Image(); pre.src = after.src; }
    };

    openLightbox = (entries, start, from) => {
      list = entries;
      opener = from || null;
      show(Math.max(0, start));
      dialog.showModal();
      document.body.classList.add('no-scroll');
    };

    if (gallery) {
      gallery.addEventListener('click', (event) => {
        const link = event.target.closest('.gallery__link');
        if (!link || !isPlainClick(event)) return;
        event.preventDefault();
        const links = items.filter((item) => !item.hidden).map((item) => $('.gallery__link', item));
        const entries = links.map((a) => {
          const thumb = $('img', a);
          return { src: a.getAttribute('href'), alt: thumb ? thumb.alt : '', caption: a.dataset.caption || '' };
        });
        openLightbox(entries, links.indexOf(link), link);
      });
    }

    $('[data-lightbox-prev]', dialog).addEventListener('click', () => show(index - 1));
    $('[data-lightbox-next]', dialog).addEventListener('click', () => show(index + 1));
    $('[data-lightbox-close]', dialog).addEventListener('click', () => dialog.close());
    dialog.addEventListener('keydown', (event) => {
      if (event.key === 'ArrowLeft') { event.preventDefault(); show(index - 1); }
      if (event.key === 'ArrowRight') { event.preventDefault(); show(index + 1); }
    });
    dialog.addEventListener('click', (event) => { if (event.target === dialog) dialog.close(); });
    dialog.addEventListener('close', () => {
      document.body.classList.remove('no-scroll');
      img.removeAttribute('src');
      if (opener) opener.focus();
    });

    let touchX = null;
    dialog.addEventListener('touchstart', (event) => { touchX = event.touches[0].clientX; }, { passive: true });
    dialog.addEventListener('touchend', (event) => {
      if (touchX === null) return;
      const dx = event.changedTouches[0].clientX - touchX;
      if (Math.abs(dx) > 50) show(index + (dx < 0 ? 1 : -1));
      touchX = null;
    });
  }

  /* ---------- wybór pojazdu ---------- */
  const garage = $('[data-garage]');
  const vehicles = Array.isArray(T.vehicles) ? T.vehicles : [];
  if (garage && vehicles.length) {
    const root = (garage.querySelector('[data-garage-shot]').getAttribute('src') || '').split('assets/')[0];
    const shotSrc = (v) => `${root}assets/shots/${v.img}.webp`;
    const shot = $('[data-garage-shot]', garage);
    const zoom = $('[data-garage-zoom]', garage);
    const name = $('[data-garage-name]', garage);
    const team = $('[data-garage-team]', garage);
    const main = $('[data-garage-pl]', garage);
    const sub = $('[data-garage-en]', garage);
    const count = $('[data-garage-count]', garage);
    const teamButtons = $$('.team-btn', garage);
    let current = 0;
    let preloaded = false;

    const preload = () => {
      if (preloaded) return;
      preloaded = true;
      vehicles.forEach((v) => { const pre = new Image(); pre.src = shotSrc(v); });
    };
    const altOf = (v) => fill(T.vehAlt, { name: v.name, team: v.teamName });
    const render = () => {
      const v = vehicles[current];
      shot.src = shotSrc(v);
      shot.alt = altOf(v);
      if (zoom) zoom.setAttribute('href', shotSrc(v));
      name.textContent = v.name;
      team.textContent = v.teamName;
      main.textContent = v.main;
      sub.textContent = v.sub || '';
      sub.hidden = !v.sub;
      count.textContent = `${current + 1} / ${vehicles.length}`;
      teamButtons.forEach((button) => {
        const on = button.dataset.team === v.team;
        button.classList.toggle('is-selected', on);
        button.setAttribute('aria-pressed', String(on));
      });
    };
    const swap = (dir) => {
      if (!moving() || !zoom) return;
      zoom.classList.remove('is-swap');
      shot.classList.remove('is-swap-next', 'is-swap-prev');
      void shot.offsetWidth; // ponowne uruchomienie animacji
      zoom.classList.add('is-swap');
      shot.classList.add(dir < 0 ? 'is-swap-prev' : 'is-swap-next');
    };
    const step = (delta) => { preload(); current = (current + delta + vehicles.length) % vehicles.length; render(); swap(delta); };

    $('[data-garage-prev]', garage).addEventListener('click', () => step(-1));
    $('[data-garage-next]', garage).addEventListener('click', () => step(1));
    teamButtons.forEach((button) => {
      button.addEventListener('click', () => {
        preload();
        const first = vehicles.findIndex((v) => v.team === button.dataset.team);
        if (first >= 0 && first !== current) { const dir = first > current ? 1 : -1; current = first; render(); swap(dir); }
      });
    });
    if (zoom && openLightbox) {
      zoom.addEventListener('click', (event) => {
        if (!isPlainClick(event)) return;
        event.preventDefault();
        const entries = vehicles.map((v) => ({ src: shotSrc(v), alt: altOf(v), caption: fill(T.vehCaption, { name: v.name, team: v.teamName }) }));
        openLightbox(entries, current, zoom);
      });
    }
    garage.addEventListener('pointerenter', preload, { once: true });
    render();
  }

  /* ---------- kopiowanie do schowka ---------- */
  $$('[data-copy]').forEach((button) => {
    const label = button.textContent;
    button.addEventListener('click', async () => {
      const text = button.dataset.copy;
      let ok = false;
      try {
        await navigator.clipboard.writeText(text);
        ok = true;
      } catch {
        const area = document.createElement('textarea');
        area.value = text;
        area.setAttribute('readonly', '');
        area.style.position = 'fixed';
        area.style.opacity = '0';
        document.body.appendChild(area);
        area.select();
        try { ok = document.execCommand('copy'); } catch { ok = false; }
        area.remove();
      }
      if (!ok) {
        const code = button.parentElement && $('code', button.parentElement);
        const selection = window.getSelection();
        if (code && selection) {
          const range = document.createRange();
          range.selectNodeContents(code);
          selection.removeAllRanges();
          selection.addRange(range);
        }
      }
      button.textContent = ok ? (T.copyOk || label) : (T.copyFail || label);
      button.classList.toggle('is-done', ok);
      window.setTimeout(() => { button.textContent = label; button.classList.remove('is-done'); }, 1800);
    });
  });
})();
