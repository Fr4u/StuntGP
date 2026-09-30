# StuntGP NEW website sources

The site in `../website` is generated from this folder. After every text change, run:

```
python build.py
```

- `template.html` — the shared page template (sections, images, scripts).
- `language.html` — the start page with a language choice styled like the game menu.
- `i18n/<code>.json` — texts for the languages: pl, en, nl, de, sv, it, es, fr.
  Every file must have exactly the same keys as `pl.json`, otherwise the build stops.
  `{{key}}` inserts HTML-escaped text, `{{{key}}}` inserts HTML unchanged.
- The mode names, their descriptions and the car descriptions on the nl/de/sv/it/es/fr pages come from the game's
  text table (`setup\languagepc.csv` in `setup.wad`).

Styles and scripts (`../website/assets/css`, `../website/assets/js`) are not generated — they are edited
directly. The game-style animations (transitions, scrolling background, logo, buttons sliding in, mode icons,
panels, countdown) take their parameters from the game's interface code, described in comments: the
`animacje jak w grze` ("animations as in the game") section in `style.css`, and `main.js` and `language.js`. The system "reduce motion"
setting turns them off.

Once a domain has been chosen, build the site with its address to add `canonical`, `hreflang`, the full
`og:image` URL and `sitemap.xml`:

```
python build.py --site-url https://your-domain.example/
```
