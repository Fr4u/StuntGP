"""Builds the multilingual StuntGP NEW website.

    python build.py [--site-url https://example.org/]

Inputs (this folder):  template.html, language.html, i18n/<code>.json
Output:                ../website/index.html (language screen) and ../website/<code>/index.html

Every {{key}} / {{{key}}} used by a template must exist in every language file,
otherwise the build stops. {{key}} is HTML-escaped, {{{key}}} is inserted as-is.
"""
import argparse
import html
import json
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parent / "website"

# Order of the language screen: Polish first, then the game's own order.
LANGS = ["pl", "en", "nl", "de", "sv", "it", "es", "fr"]

VEHICLES = [  # key, team id, team name, vehicle name, screenshot
    ("mean_streak", "wild", "Wild Wheels", "Mean Streak", "r20-vehicle-mean-streak"),
    ("alley_kat", "wild", "Wild Wheels", "Alley Kat", "r20-vehicle-alley-kat"),
    ("stunt_monkey", "aero", "Aero Blasters", "Stunt Monkey", "r20-vehicle-stunt-monkey"),
    ("the_brute", "speed", "Speed Demons", "The Brute", "r20-vehicle-the-brute"),
    ("sand_shark", "speed", "Speed Demons", "Sand Shark", "r20-vehicle-sand-shark"),
]

TOKEN = re.compile(r"\{\{\{\s*([A-Za-z0-9_]+)\s*\}\}\}|\{\{\s*([A-Za-z0-9_]+)\s*\}\}")


def load(code):
    data = json.loads((HERE / "i18n" / f"{code}.json").read_text(encoding="utf-8"))
    for section in ("meta", "strings", "vehicles"):
        if section not in data:
            sys.exit(f"{code}.json: missing section '{section}'")
    return data


def render(template, values, where):
    missing = set()

    def sub(m):
        raw_key, esc_key = m.group(1), m.group(2)
        key = raw_key or esc_key
        if key not in values:
            missing.add(key)
            return ""
        value = values[key]
        return value if raw_key else html.escape(value, quote=True)

    out = TOKEN.sub(sub, template)
    if missing:
        sys.exit(f"{where}: missing keys: {', '.join(sorted(missing))}")
    return out


def used_keys(template):
    return {m.group(1) or m.group(2) for m in TOKEN.finditer(template)}


def fmt(pattern, **kw):
    for k, v in kw.items():
        pattern = pattern.replace("{" + k + "}", v)
    return pattern



def bust(page):
    """Append ?v=<content hash> to local CSS/JS links so a new deployment never pairs new HTML with cached old files."""
    import hashlib

    def repl(m):
        path = OUT / "assets" / m.group(1) / m.group(2)
        if not path.is_file():
            return m.group(0)
        return m.group(0)[:-1] + "?v=" + hashlib.sha256(path.read_bytes()).hexdigest()[:10] + '"'

    return re.sub(r'assets/(css|js)/([\w.-]+\.(?:css|js))"', repl, page)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--site-url", default="", help="absolute site URL, e.g. https://example.org/ (enables canonical/hreflang)")
    args = ap.parse_args()
    site = args.site_url.rstrip("/") + "/" if args.site_url else ""

    page_tpl = (HERE / "template.html").read_text(encoding="utf-8")
    screen_tpl = (HERE / "language.html").read_text(encoding="utf-8")
    data = {code: load(code) for code in LANGS}

    # every language must provide exactly the same string keys as Polish
    base_keys = set(data["pl"]["strings"])
    for code in LANGS:
        keys = set(data[code]["strings"])
        if keys != base_keys:
            sys.exit(f"{code}.json: missing {sorted(base_keys - keys)} extra {sorted(keys - base_keys)}")
        vkeys = set(data[code]["vehicles"])
        if vkeys != {v[0] for v in VEHICLES}:
            sys.exit(f"{code}.json: vehicles mismatch {sorted(vkeys)}")

    special = {"ROOT", "OG_IMAGE", "HEAD_LINKS", "LANG_MENU", "I18N_JSON", "VEH1_SUB_HIDDEN"}
    derived = {"html_lang", "og_locale", "lang_code", "veh1_main", "veh1_sub_shown", "veh1_alt"} | \
              {f"mode{i}_help_sub_shown" for i in range(1, 6)}
    unused = base_keys - used_keys(page_tpl) - {k for k in base_keys if k.startswith(("js_", "screen_"))}
    if unused:
        print("warning: unused keys:", ", ".join(sorted(unused)))

    for code in LANGS:
        d = data[code]
        s = dict(d["strings"])
        root = "../"
        s.update({
            "ROOT": root,
            "html_lang": code,
            "og_locale": d["meta"]["og_locale"],
            "lang_code": code.upper(),
        })
        for i in range(1, 6):
            sub = s[f"mode{i}_help_sub"]
            s[f"mode{i}_help_sub_shown"] = f"({sub})" if sub else ""
        vehicles = []
        for key, team, team_name, name, img in VEHICLES:
            v = d["vehicles"][key]
            vehicles.append({"team": team, "teamName": team_name, "name": name, "img": img,
                             "main": v["main"], "sub": v["sub"]})
        first = vehicles[0]
        s["veh1_main"] = first["main"]
        s["veh1_sub_shown"] = first["sub"]
        s["veh1_alt"] = fmt(s["js_veh_alt"], name=first["name"], team=first["teamName"])
        s["VEH1_SUB_HIDDEN"] = "" if first["sub"] else " hidden"

        menu = []
        for other in LANGS:
            cur = ' aria-current="page"' if other == code else ""
            name = html.escape(data[other]["meta"]["lang_name"])
            menu.append(f'          <li><a href="{root}{other}/" hreflang="{other}" lang="{other}" data-lang="{other}"{cur}>'
                        f'<img src="{root}assets/img/flags/{other}.webp" width="212" height="128" alt=""><span>{name}</span></a></li>')
        s["LANG_MENU"] = "\n".join(menu)

        if site:
            links = [f'  <link rel="canonical" href="{site}{code}/">']
            links += [f'  <link rel="alternate" hreflang="{o}" href="{site}{o}/">' for o in LANGS]
            links.append(f'  <link rel="alternate" hreflang="x-default" href="{site}">')
            s["HEAD_LINKS"] = "\n".join(links)
            s["OG_IMAGE"] = f"{site}assets/img/og-image.jpg"
        else:
            s["HEAD_LINKS"] = ""
            s["OG_IMAGE"] = f"{root}assets/img/og-image.jpg"

        i18n = {
            "lang": code,
            "slideDot": s["js_slide_dot"],
            "copyOk": s["js_copy_ok"],
            "copyFail": s["js_copy_fail"],
            "vehAlt": s["js_veh_alt"],
            "vehCaption": s["js_veh_caption"],
            "vehicles": vehicles,
        }
        s["I18N_JSON"] = json.dumps(i18n, ensure_ascii=False).replace("</", "<\\/")

        page = render(page_tpl, s, f"{code}/index.html")
        target = OUT / code / "index.html"
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(bust(page), encoding="utf-8", newline="\n")
        print("wrote", target.relative_to(OUT.parent))

    # ---- language screen (site root)
    buttons, flags, screen = [], [], {}
    for i, code in enumerate(LANGS):
        d = data[code]
        name = html.escape(d["meta"]["lang_name"])
        # White entry layer of the game's menu rows; the bottom row starts first, each row above one step later.
        flash = f'<span class="fly-flash" aria-hidden="true" style="--fly-i:{len(LANGS) - 1 - i}"></span>'
        buttons.append(f'        <li><a class="menu-btn lang-btn" href="{code}/" hreflang="{code}" lang="{code}" data-lang="{code}" data-index="{i}">{name}{flash}</a></li>')
        flags.append(f'      <img class="lang-flag" src="assets/img/flags/{code}.webp" width="212" height="128" alt="" data-lang="{code}" data-index="{i}">')
        screen[code] = {"hint": d["strings"]["screen_hint"], "legal": d["strings"]["hero_legal"],
                        "title": d["strings"]["screen_title"]}
    sv = {
        "LANG_BUTTONS": "\n".join(buttons),
        "LANG_FLAGS": "\n".join(flags),
        "SCREEN_JSON": json.dumps({"order": LANGS, "texts": screen}, ensure_ascii=False).replace("</", "<\\/"),
        "hint_pl": data["pl"]["strings"]["screen_hint"],
        "legal_pl": data["pl"]["strings"]["hero_legal"],
        "title_all": " · ".join(data[c]["strings"]["screen_title"] for c in LANGS),
        "HEAD_LINKS": "\n".join([f'  <link rel="alternate" hreflang="{o}" href="{site}{o}/">' for o in LANGS]
                                + [f'  <link rel="canonical" href="{site}">']) if site else "",
        "OG_IMAGE": f"{site}assets/img/og-image.jpg" if site else "assets/img/og-image.jpg",
    }
    (OUT / "index.html").write_text(bust(render(screen_tpl, sv, "index.html")), encoding="utf-8", newline="\n")
    print("wrote website/index.html")

    if site:
        urls = [site] + [f"{site}{c}/" for c in LANGS]
        xml = ['<?xml version="1.0" encoding="UTF-8"?>',
               '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">']
        xml += [f"  <url><loc>{u}</loc></url>" for u in urls]
        xml.append("</urlset>")
        (OUT / "sitemap.xml").write_text("\n".join(xml) + "\n", encoding="utf-8", newline="\n")
        print("wrote website/sitemap.xml")


if __name__ == "__main__":
    main()
