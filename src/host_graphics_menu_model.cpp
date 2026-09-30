#include "sgn/host_graphics_menu_model.hpp"
#include "sgn/host_texture_disk_cache.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>

// NEW HOST (R24) shared GRAPHICS menu model; see host_graphics_menu_model.hpp.
namespace sgn::host_graphics_menu
{
using host_graphics::DisplayMode;
using host_graphics::PostAntiAliasing;
using host_graphics::Settings;
using host_graphics::TextureFilter;

// ---------------------------------------------------------------- session
namespace
{
struct Session
{
    std::mutex mutex;
    std::optional<std::filesystem::path> root;
    Settings persisted;
    SessionOverrides pending; // overrides the user has not changed in a menu
};
Session &session()
{
    static Session s;
    return s;
}
void consume_override(bool SessionOverrides::*field)
{
    auto &s = session();
    std::lock_guard lock(s.mutex);
    s.pending.*field = false;
}
void consume_all_overrides()
{
    auto &s = session();
    std::lock_guard lock(s.mutex);
    s.pending = {};
}
Settings without_pending(Settings value, const Settings &persisted, const SessionOverrides &pending)
{
    if (pending.interpolation)
        value.interpolation = persisted.interpolation;
    if (pending.vsync)
        value.vsync = persisted.vsync;
    if (pending.original_4x3)
        value.original_4x3 = persisted.original_4x3;
    return value;
}
} // namespace
void begin_session(const std::filesystem::path &game_root, const Settings &persisted, SessionOverrides overrides)
{
    if (game_root.empty())
        throw std::invalid_argument("host graphics session requires the game root");
    auto &s = session();
    {
        std::lock_guard lock(s.mutex);
        s.root = game_root;
        s.persisted = host_graphics::sanitize(persisted);
        s.pending = overrides;
    }
    // NEW HOST (R30): the persistent converted-texture cache lives next to the settings file
    // (<root>/settings/texture-cache). See host_texture_disk_cache.hpp (SGN_TEXTURE_CACHE=0 disables it).
    host_texture_disk_cache::configure(host_graphics::default_path(game_root).parent_path() / "texture-cache");
}
void report(const std::filesystem::path &game_root, std::string_view line)
{
    const auto log = game_root / "reports/source-menu-session.log";
    std::error_code error;
    std::filesystem::create_directories(log.parent_path(), error);
    // A diagnostic line cannot replace the reported outcome; an unwritable
    // reports directory leaves only the returned status of the caller.
    std::ofstream(log, std::ios::app) << "host graphics: " << line << '\n';
}
void save_settings()
{
    auto &s = session();
    std::filesystem::path root;
    Settings persisted;
    SessionOverrides pending;
    {
        std::lock_guard lock(s.mutex);
        if (!s.root)
            throw std::logic_error("GRAPHICS menu left without a registered host graphics session");
        root = *s.root;
        persisted = s.persisted;
        pending = s.pending;
    }
    const auto path = host_graphics::default_path(root);
    // Untouched session overrides keep the file's values; everything else
    // (display_mode included, which has no override) is the user's current value.
    const auto user = without_pending(host_graphics::current(), persisted, pending);
    if (host_graphics::save(path, user))
    {
        std::lock_guard lock(s.mutex);
        s.persisted = without_pending(host_graphics::current(), s.persisted, s.pending);
    }
    else
        report(root, "settings could not be written to " + path.string());
}

// ---------------------------------------------------------------- values
namespace
{
std::uint32_t anisotropic_step(std::uint32_t anisotropy)
{
    return anisotropy < 4u ? 0u : anisotropy < 8u ? 1u : anisotropy < 16u ? 2u : 3u;
}
std::uint32_t wrap(std::uint32_t index, std::uint32_t count, int direction)
{
    return (index + count + static_cast<std::uint32_t>(direction)) % count;
}
// Frame limit list (0 = off). A value from the file that is not in the list is
// shown as-is; a step goes to the next list entry in that direction.
constexpr std::array<std::uint32_t, 8> frame_limits{0, 60, 120, 144, 165, 237, 240, 360};
std::uint32_t step_frame_limit(std::uint32_t value, int direction)
{
    const auto at = std::find(frame_limits.begin(), frame_limits.end(), value);
    if (at != frame_limits.end())
        return frame_limits[wrap(static_cast<std::uint32_t>(at - frame_limits.begin()),
                                 static_cast<std::uint32_t>(frame_limits.size()), direction)];
    if (direction > 0)
    {
        const auto above = std::upper_bound(frame_limits.begin(), frame_limits.end(), value);
        return above == frame_limits.end() ? frame_limits.front() : *above;
    }
    const auto below = std::lower_bound(frame_limits.begin(), frame_limits.end(), value);
    return *(below - 1); // value > 0 here, so an entry below exists
}
} // namespace
void change(Row row, int direction)
{
    if (direction != 1 && direction != -1)
        throw std::invalid_argument("GRAPHICS value step must be +1 or -1");
    auto s = host_graphics::current(); // never a cached value: Alt+Enter also calls set()
    switch (row)
    {
    case Row::display_mode:
        s.display_mode = static_cast<DisplayMode>(wrap(static_cast<std::uint32_t>(s.display_mode), 3, direction));
        break;
    case Row::interpolation:
        s.interpolation = !s.interpolation;
        consume_override(&SessionOverrides::interpolation);
        break;
    case Row::vsync:
        s.vsync = !s.vsync;
        consume_override(&SessionOverrides::vsync);
        break;
    case Row::aspect:
        s.original_4x3 = !s.original_4x3;
        consume_override(&SessionOverrides::original_4x3);
        break;
    case Row::anti_aliasing: {
        constexpr std::array<std::uint32_t, 4> samples{1, 2, 4, 8};
        const auto at = std::find(samples.begin(), samples.end(), s.msaa_samples);
        if (at == samples.end())
            throw std::logic_error("host MSAA count outside its sanitized domain");
        s.msaa_samples = samples[wrap(static_cast<std::uint32_t>(at - samples.begin()), 4, direction)];
        break;
    }
    case Row::texture_filter: {
        // Original, Trilinear, Anisotropic 2x/4x/8x/16x.
        auto index = s.texture_filter == TextureFilter::original    ? 0u
                     : s.texture_filter == TextureFilter::trilinear ? 1u
                                                                    : 2u + anisotropic_step(s.anisotropy);
        index = wrap(index, 6, direction);
        if (index == 0)
            s.texture_filter = TextureFilter::original;
        else if (index == 1)
            s.texture_filter = TextureFilter::trilinear;
        else
        {
            s.texture_filter = TextureFilter::anisotropic;
            s.anisotropy = 2u << (index - 2u);
        }
        break;
    }
    case Row::mipmaps:
        s.generate_mipmaps = !s.generate_mipmaps;
        break;
    case Row::alpha_to_coverage:
        s.alpha_to_coverage = !s.alpha_to_coverage;
        break;
    case Row::fxaa:
        s.post_anti_aliasing =
            s.post_anti_aliasing == PostAntiAliasing::fxaa ? PostAntiAliasing::off : PostAntiAliasing::fxaa;
        break;
    case Row::smooth_gradients:
        s.smooth_gradients = !s.smooth_gradients;
        break;
    case Row::texture_edge_fix:
        s.texture_edge_fix = !s.texture_edge_fix;
        break;
    case Row::frame_rate_limit:
        s.frame_rate_limit = step_frame_limit(s.frame_rate_limit, direction);
        break;
    case Row::low_latency:
        s.low_latency = !s.low_latency;
        break;
    case Row::high_precision_textures:
        s.high_precision_textures = !s.high_precision_textures;
        break;
    case Row::sharp_hud:
        s.sharp_hud = !s.sharp_hud;
        break;
    case Row::texture_sharpness: // -3..+3 with wrap-around, like the other lists
        s.texture_sharpness =
            static_cast<std::int32_t>(
                wrap(static_cast<std::uint32_t>(std::clamp(s.texture_sharpness, -3, 3) + 3), 7, direction)) -
            3;
        break;
    case Row::smooth_magnification:
        s.smooth_magnification = !s.smooth_magnification;
        break;
    case Row::bloom:
        s.bloom = !s.bloom;
        break;
    case Row::smooth_effects:
        s.smooth_effects = !s.smooth_effects;
        break;
    case Row::reset:
        s = Settings{};
        consume_all_overrides();
        break;
    default:
        throw std::logic_error("host GRAPHICS row outside its table");
    }
    host_graphics::set(s);
}

std::span<const Settings> value_variants()
{
    static const auto variants = [] {
        std::array<Settings, 16> v{};
        v[0].display_mode = DisplayMode::windowed;
        v[1].display_mode = DisplayMode::borderless;
        v[2].display_mode = DisplayMode::exclusive;
        v[3].interpolation = false;
        v[3].vsync = false;
        v[3].original_4x3 = true;
        v[3].msaa_samples = 1;
        v[3].generate_mipmaps = false;
        v[3].alpha_to_coverage = false;
        v[3].smooth_gradients = false;
        v[3].texture_edge_fix = false;
        v[3].low_latency = true;
        v[3].high_precision_textures = false;
        v[3].sharp_hud = false;
        v[3].smooth_magnification = true;
        v[3].bloom = true;
        v[3].smooth_effects = false;
        v[4].smooth_magnification = false; // defaults already cover the other state
        v[14].texture_sharpness = -3;
        v[15].texture_sharpness = 3;
        v[4].msaa_samples = 2;
        v[5].msaa_samples = 8;
        v[6].texture_filter = TextureFilter::original;
        v[7].texture_filter = TextureFilter::trilinear;
        v[8].anisotropy = 16;
        v[9].post_anti_aliasing = PostAntiAliasing::fxaa;
        v[10].anisotropy = 2;
        v[11].anisotropy = 8;
        v[12].frame_rate_limit = 360;
        v[13].frame_rate_limit = 1000; // widest value a file may hold
        return v;
    }();
    return variants;
}

// ---------------------------------------------------------------- strings
// Compact labels (<= 13 bytes) and values (<= 11 bytes): the pause panel puts
// "* LABEL" before the original 0x430E6667 (142.4) value column and the value
// between that column and the right inner margin (89.6 units in the 256-unit
// box). The Options page uses the same strings ("LABEL: VALUE").
struct Language
{
    std::u8string_view catalog_name; // LANGUAGE NAME column, as 440AE0 stores it
    std::u8string_view options_row, options_help, title, back, on, off, separator;
    std::array<std::u8string_view, row_count> labels, helps;
    std::array<std::u8string_view, 3> display_modes; // windowed, fullscreen windowed (borderless), exclusive
    std::u8string_view widescreen, original_4x3, original, trilinear, anisotropic;
    std::u8string_view mipmaps_note; // short pause-panel note under the rows
    std::u8string_view advanced;     // R29: pause row in CAMERA's slot (<= 11 bytes)
};
namespace
{
constexpr std::array<Language, 7> languages{{
    {u8"English",
     u8"GRAPHICS",
     u8"Adjust modern display and image quality settings.",
     u8"GRAPHICS",
     u8"BACK",
     u8"ON",
     u8"OFF",
     u8": ",
     {u8"DISPLAY MODE",  u8"INTERPOLATE", u8"VSYNC",        u8"ASPECT",      u8"ANTI-ALIAS",
      u8"FILTERING",     u8"MIPMAPS",     u8"ALPHA-TO-COV", u8"FXAA",        u8"SMOOTH GRAD.",
      u8"TEXTURE EDGES", u8"FRAME LIMIT", u8"LOW LATENCY",  u8"HQ TEXTURES", u8"SHARP HUD",
      u8"SHARPNESS",     u8"SMOOTH ZOOM", u8"GLOW",         u8"SMOOTH FX",   u8"RESET TO DEFAULTS"},
     {u8"Window, borderless fullscreen or exclusive fullscreen.",
      u8"Smooth motion on high refresh rate displays.",
      u8"Synchronise frames with the display to prevent tearing.",
      u8"Widescreen view or the original 4:3 picture.",
      u8"Smooth jagged polygon edges with multisampling.",
      u8"Sharper textures in the distance and at angles.",
      u8"Build missing mipmaps. Applies from the next track.",
      u8"Smoother fence and foliage edges (needs anti-aliasing).",
      u8"Fast post-process smoothing of the whole image.",
      u8"Dither colour gradients instead of visible banding.",
      u8"No dark halos on cut-out edges. Applies from the next track.",
      u8"Cap the frame rate to save power and heat.",
      u8"Less input lag, less headroom for frame time spikes.",
      u8"Full colour depth textures. Applies from the next track.",
      u8"Crisper HUD and fonts. Applies from the next track.",
      u8"Texture detail: higher is sharper, lower softer.",
      u8"Higher quality filtering for magnified textures.",
      u8"Glow around the game's lights, trails and flares.",
      u8"Trails, particles and shadows move at display rate.",
      u8"Restore the recommended graphics settings."},
     {u8"WINDOW", u8"BORDERLESS", u8"EXCLUSIVE"},
     u8"WIDE",
     u8"4:3",
     u8"ORIGINAL",
     u8"TRILINEAR",
     u8"ANISO",
     u8"TAKES EFFECT ON THE NEXT TRACK",
     u8"ADVANCED"},
    {u8"Nederlands",
     u8"GRAFISCHE OPTIES",
     u8"Pas moderne beeld- en kwaliteitsinstellingen aan.",
     u8"GRAFISCHE OPTIES",
     u8"TERUG",
     u8"AAN",
     u8"UIT",
     u8": ",
     {u8"WEERGAVE",      u8"INTERPOLATIE", u8"VSYNC",         u8"FORMAAT",      u8"ANTI-ALIAS",
      u8"FILTER",        u8"MIPMAPS",      u8"ALPHA-TO-COV",  u8"FXAA",         u8"KLEURVERLOOP",
      u8"TEXTUURRANDEN", u8"FPS-LIMIET",   u8"LAGE LATENTIE", u8"HQ-TEXTUREN",  u8"SCHERPE HUD",
      u8"SCHERPTE",      u8"ZACHTE ZOOM",  u8"GLOED",         u8"VLOEIENDE FX", u8"STANDAARDWAARDEN"},
     {u8"Venster, randloos volledig scherm of exclusief.",
      u8"Vloeiende beweging op schermen met hoge verversing.",
      u8"Synchroniseer beelden met het scherm tegen beeldscheuring.",
      u8"Breedbeeld of het originele 4:3-beeld.",
      u8"Maak gekartelde randen glad met multisampling.",
      u8"Scherpere texturen op afstand en onder een hoek.",
      u8"Maakt ontbrekende mipmaps. Actief vanaf de volgende baan.",
      u8"Gladdere randen van hekken en bladeren (met anti-aliasing).",
      u8"Snelle nabewerking die het hele beeld gladstrijkt.",
      u8"Kleurverlopen zonder zichtbare banden (dithering).",
      u8"Geen donkere randjes. Actief vanaf de volgende baan.",
      u8"Beperk de framerate om stroom en warmte te sparen.",
      u8"Minder invoervertraging, minder marge voor pieken.",
      u8"Volle kleurdiepte voor texturen. Vanaf de volgende baan.",
      u8"Scherpere HUD en tekst. Actief vanaf de volgende baan.",
      u8"Textuurdetail: hoger is scherper, lager zachter.",
      u8"Betere filtering voor vergrote texturen.",
      u8"Gloed rond lichten, sporen en flitsen van het spel.",
      u8"Sporen, deeltjes en schaduwen bewegen op schermfrequentie.",
      u8"Herstel de aanbevolen grafische instellingen."},
     {u8"VENSTER", u8"RANDLOOS", u8"EXCLUSIEF"},
     u8"BREED",
     u8"4:3",
     u8"ORIGINEEL",
     u8"TRILINEAIR",
     u8"ANISO",
     u8"ACTIEF VANAF DE VOLGENDE BAAN",
     u8"GEAVANCEERD"},
    {u8"Deutsch",
     u8"GRAFIK",
     u8"Moderne Grafik- und Bildqualität einstellen.",
     u8"GRAFIK",
     u8"ZURÜCK",
     u8"AN",
     u8"AUS",
     u8": ",
     {u8"ANZEIGE",      u8"ZWISCHENBILD", u8"VSYNC",         u8"FORMAT",      u8"ANTI-ALIAS",
      u8"TEXTURFILTER", u8"MIPMAPS",      u8"ALPHA-TO-COV",  u8"FXAA",        u8"FARBVERLÄUFE",
      u8"TEXTURKANTEN", u8"FPS-LIMIT",    u8"NIEDR. LATENZ", u8"HQ-TEXTUREN", u8"SCHARFES HUD",
      u8"SCHÄRFE",      u8"WEICHER ZOOM", u8"LEUCHTEN",      u8"FLÜSSIGE FX", u8"STANDARDWERTE"},
     {u8"Fenster, randloses Vollbild oder exklusives Vollbild.",
      u8"Flüssige Bewegung bei hoher Bildwiederholrate.",
      u8"Synchronisiert Bilder mit dem Bildschirm gegen Bildrisse.",
      u8"Breitbild oder das originale 4:3-Bild.",
      u8"Glättet gezackte Polygonkanten durch Multisampling.",
      u8"Schärfere Texturen in der Ferne und aus flachen Winkeln.",
      u8"Erzeugt fehlende Mipmaps. Wirkt ab der nächsten Strecke.",
      u8"Glattere Kanten an Zäunen und Laub (mit Kantenglättung).",
      u8"Schnelle Nachbearbeitung, die das ganze Bild glättet.",
      u8"Farbverläufe ohne sichtbare Stufen (Dithering).",
      u8"Keine dunklen Kantensäume. Wirkt ab der nächsten Strecke.",
      u8"Begrenzt die Bildrate, spart Strom und Wärme.",
      u8"Weniger Eingabeverzögerung, weniger Reserve für Spitzen.",
      u8"Volle Farbtiefe für Texturen. Ab der nächsten Strecke.",
      u8"Schärferes HUD und Schrift. Ab der nächsten Strecke.",
      u8"Texturdetail: höher ist schärfer, niedriger weicher.",
      u8"Bessere Filterung für vergrößerte Texturen.",
      u8"Leuchten um Lichter, Spuren und Blendeffekte des Spiels.",
      u8"Spuren, Partikel und Schatten mit Bildwiederholrate.",
      u8"Stellt die empfohlenen Grafikeinstellungen wieder her."},
     {u8"FENSTER", u8"RAHMENLOS", u8"EXKLUSIV"},
     u8"BREIT",
     u8"4:3",
     u8"ORIGINAL",
     u8"TRILINEAR",
     u8"ANISO",
     u8"WIRKT AB DER NÄCHSTEN STRECKE",
     u8"ERWEITERT"},
    {u8"Svenska",
     u8"GRAFIK",
     u8"Justera moderna inställningar för bildkvalitet.",
     u8"GRAFIK",
     u8"TILLBAKA",
     u8"PÅ",
     u8"AV",
     u8": ",
     {u8"VISNING",      u8"INTERPOLERA", u8"VSYNC",        u8"BILDFORMAT",  u8"ANTI-ALIAS",
      u8"TEXTURFILTER", u8"MIPMAPPAR",   u8"ALPHA-TO-COV", u8"FXAA",        u8"ÖVERGÅNGAR",
      u8"TEXTURKANTER", u8"FPS-GRÄNS",   u8"LÅG LATENS",   u8"HQ-TEXTURER", u8"SKARP HUD",
      u8"SKÄRPA",       u8"MJUK ZOOM",   u8"GLÖD",         u8"MJUKA FX",    u8"ÅTERSTÄLL STANDARD"},
     {u8"Fönster, kantlöst helskärmsfönster eller exklusiv helskärm.",
      u8"Mjuk rörelse på skärmar med hög uppdateringsfrekvens.",
      u8"Synkronisera bilder med skärmen mot bildrivning.",
      u8"Bredbild eller den ursprungliga 4:3-bilden.",
      u8"Jämna ut taggiga polygonkanter med multisampling.",
      u8"Skarpare texturer på avstånd och i vinkel.",
      u8"Skapar saknade mipmappar. Gäller från nästa bana.",
      u8"Mjukare kanter på staket och lövverk (med kantutjämning).",
      u8"Snabb efterbehandling som jämnar ut hela bilden.",
      u8"Färgövergångar utan synliga band (dithering).",
      u8"Inga mörka kanter på urklipp. Gäller från nästa bana.",
      u8"Begränsa bildfrekvensen för mindre ström och värme.",
      u8"Mindre fördröjning, mindre marginal för toppar.",
      u8"Texturer med full färgdjup. Gäller från nästa bana.",
      u8"Skarpare HUD och text. Gäller från nästa bana.",
      u8"Texturdetalj: högre är skarpare, lägre mjukare.",
      u8"Bättre filtrering för förstorade texturer.",
      u8"Glöd kring spelets ljus, spår och reflexer.",
      u8"Spår, partiklar och skuggor rör sig i skärmens takt.",
      u8"Återställ de rekommenderade grafikinställningarna."},
     {u8"FÖNSTER", u8"KANTLÖST", u8"EXKLUSIV"},
     u8"BRED",
     u8"4:3",
     u8"ORIGINAL",
     u8"TRILINJÄR",
     u8"ANISO",
     u8"GÄLLER FRÅN NÄSTA BANA",
     u8"AVANCERAT"},
    {u8"Italiano",
     u8"GRAFICA",
     u8"Per regolare le opzioni moderne di qualità grafica.",
     u8"GRAFICA",
     u8"INDIETRO",
     u8"ON",
     u8"OFF",
     u8": ",
     {u8"SCHERMO",       u8"INTERPOLARE",  u8"VSYNC",         u8"FORMATO",    u8"ANTIALIAS",
      u8"TEXTURE",       u8"MIPMAP",       u8"ALPHA-TO-COV",  u8"FXAA",       u8"SFUMATURE",
      u8"BORDI TEXTURE", u8"LIMITE FPS",   u8"BASSA LATENZA", u8"TEXTURE HQ", u8"HUD NITIDO",
      u8"NITIDEZZA",     u8"ZOOM MORBIDO", u8"BAGLIORE",      u8"FX FLUIDI",  u8"VALORI PREDEFINITI"},
     {u8"Finestra, schermo intero senza bordi o esclusivo.",
      u8"Movimento fluido sugli schermi ad alta frequenza.",
      u8"Sincronizza i fotogrammi con lo schermo contro il tearing.",
      u8"Visuale panoramica o immagine originale in 4:3.",
      u8"Per smussare i bordi seghettati con il multisampling.",
      u8"Texture più nitide in lontananza e in prospettiva.",
      u8"Crea le mipmap mancanti. Attive dal prossimo tracciato.",
      u8"Bordi più morbidi per reti e fogliame (con antialiasing).",
      u8"Rapida elaborazione che smussa l'intera immagine.",
      u8"Sfumature senza bande visibili (dithering).",
      u8"Niente aloni scuri sui bordi. Attivo dal prossimo tracciato.",
      u8"Limita i fotogrammi per risparmiare energia e calore.",
      u8"Meno ritardo dei comandi, meno margine per i picchi.",
      u8"Texture a colori pieni. Attivo dal prossimo tracciato.",
      u8"HUD e testo più nitidi. Attivo dal prossimo tracciato.",
      u8"Dettaglio: più alto più nitido, più basso più morbido.",
      u8"Filtraggio migliore per le texture ingrandite.",
      u8"Bagliore attorno a luci, scie e riflessi del gioco.",
      u8"Scie, particelle e ombre alla frequenza dello schermo.",
      u8"Per ripristinare le impostazioni grafiche consigliate."},
     {u8"FINESTRA", u8"NO BORDI", u8"ESCLUSIVO"},
     u8"LARGO",
     u8"4:3",
     u8"ORIGINALE",
     u8"TRILINEARE",
     u8"ANISO",
     u8"ATTIVO DAL PROSSIMO TRACCIATO",
     u8"AVANZATE"},
    {u8"Espanol",
     u8"GRÁFICOS",
     u8"Ajustar las opciones modernas de calidad gráfica.",
     u8"GRÁFICOS",
     u8"VOLVER",
     u8"SÍ",
     u8"NO",
     u8": ",
     {u8"PANTALLA",     u8"INTERPOLAR", u8"VSYNC",         u8"FORMATO",     u8"ANTIALIAS",
      u8"TEXTURAS",     u8"MIPMAPS",    u8"ALPHA-TO-COV",  u8"FXAA",        u8"DEGRADADOS",
      u8"BORDES TEXT.", u8"LÍMITE FPS", u8"BAJA LATENCIA", u8"TEXTURAS HQ", u8"HUD NÍTIDO",
      u8"NITIDEZ",      u8"ZOOM SUAVE", u8"RESPLANDOR",    u8"FX SUAVES",   u8"PREDETERMINADOS"},
     {u8"Ventana, pantalla completa sin bordes o exclusiva.",
      u8"Movimiento suave en pantallas de alta frecuencia.",
      u8"Sincronizar las imágenes con la pantalla para evitar cortes.",
      u8"Imagen panorámica o la imagen original en 4:3.",
      u8"Suavizar los bordes dentados con multimuestreo.",
      u8"Texturas más nítidas a distancia y en ángulo.",
      u8"Crea mipmaps que faltan. Se aplica en el próximo circuito.",
      u8"Bordes más suaves en vallas y follaje (con antialiasing).",
      u8"Suavizado rápido de toda la imagen tras el renderizado.",
      u8"Degradados sin bandas visibles (tramado).",
      u8"Sin halos oscuros en bordes. Se aplica al próximo circuito.",
      u8"Limita la frecuencia de imagen para ahorrar energía.",
      u8"Menos retraso de control, menos margen para picos.",
      u8"Texturas con color completo. Se aplica al próximo circuito.",
      u8"HUD y texto más nítidos. Se aplica al próximo circuito.",
      u8"Detalle: más alto más nítido, más bajo más suave.",
      u8"Mejor filtrado para texturas ampliadas.",
      u8"Resplandor en luces, estelas y destellos del juego.",
      u8"Estelas, partículas y sombras a la frecuencia de pantalla.",
      u8"Restaurar la configuración gráfica recomendada."},
     {u8"VENTANA", u8"SIN BORDES", u8"EXCLUSIVA"},
     u8"ANCHA",
     u8"4:3",
     u8"ORIGINAL",
     u8"TRILINEAL",
     u8"ANISO",
     u8"SE APLICA EN EL PRÓXIMO CIRCUITO",
     u8"AVANZADO"},
    {u8"Francais",
     u8"GRAPHISMES",
     u8"Modifiez les options modernes de qualité graphique.",
     u8"GRAPHISMES",
     u8"RETOUR",
     u8"OUI",
     u8"NON",
     u8" : ",
     {u8"AFFICHAGE",     u8"INTERPOLER", u8"VSYNC",         u8"FORMAT",      u8"LISSAGE",
      u8"FILTRAGE",      u8"MIPMAPS",    u8"ALPHA-TO-COV",  u8"FXAA",        u8"DEGRADES",
      u8"BORDS TEXTURE", u8"LIMITE FPS", u8"BASSE LATENCE", u8"TEXTURES HQ", u8"HUD NET",
      u8"NETTETE",       u8"ZOOM LISSE", u8"LUEUR",         u8"FX FLUIDES",  u8"VALEURS PAR DEFAUT"},
     {u8"Fenêtre, plein écran sans bordure ou exclusif.",
      u8"Mouvement fluide sur les écrans à haute fréquence.",
      u8"Synchronise les images avec l'écran contre le déchirement.",
      u8"Image grand écran ou image originale en 4:3.",
      u8"Lisse les bords crénelés par multi-échantillonnage.",
      u8"Textures plus nettes au loin et en biais.",
      u8"Crée les mipmaps manquantes. Actif au prochain circuit.",
      u8"Bords plus doux des grillages et feuillages (anticrénelage).",
      u8"Lissage rapide de toute l'image en post-traitement.",
      u8"Dégradés sans bandes visibles (tramage).",
      u8"Pas de halos sombres aux bords. Actif au prochain circuit.",
      u8"Limite la fréquence d'images pour économiser l'énergie.",
      u8"Moins de latence, moins de marge pour les pics.",
      u8"Textures en couleurs complètes. Actif au prochain circuit.",
      u8"HUD et texte plus nets. Actif au prochain circuit.",
      u8"Détail : plus haut plus net, plus bas plus doux.",
      u8"Meilleur filtrage des textures agrandies.",
      u8"Lueur autour des lumières, traînées et reflets du jeu.",
      u8"Traînées, particules et ombres à la fréquence de l'écran.",
      u8"Rétablit les réglages graphiques recommandés."},
     {u8"FENETRE", u8"SANS BORD", u8"EXCLUSIF"},
     u8"LARGE",
     u8"4:3",
     u8"ORIGINAL",
     u8"TRILINEAIRE",
     u8"ANISO",
     u8"ACTIF AU PROCHAIN CIRCUIT",
     u8"AVANCE"},
}};
std::size_t index(Row row)
{
    const auto value = static_cast<std::size_t>(row);
    if (value >= row_count)
        throw std::logic_error("host GRAPHICS row outside its table");
    return value;
}
} // namespace
const Language &language_for_catalog_name(std::u8string_view name)
{
    for (const auto &language : languages)
        if (language.catalog_name == name)
            return language;
    return languages[0];
}
const Language &language_for_flag(std::uint32_t flag)
{
    // Retail languagePC.csv: LANGUAGE FLAG row 1..7 in the column order above.
    return flag >= 1u && flag <= languages.size() ? languages[flag - 1u] : languages[0];
}
std::u8string_view options_row_label(const Language &l)
{
    return l.options_row;
}
std::u8string_view options_row_help(const Language &l)
{
    return l.options_help;
}
std::u8string_view title(const Language &l)
{
    return l.title;
}
std::u8string_view advanced_label(const Language &l)
{
    return l.advanced;
}
std::u8string_view back_label(const Language &l)
{
    return l.back;
}
std::u8string_view label(const Language &l, Row row)
{
    return l.labels[index(row)];
}
std::u8string_view help(const Language &l, Row row)
{
    return l.helps[index(row)];
}
std::u8string_view separator(const Language &l)
{
    return l.separator;
}
std::u8string_view pause_note(const Language &l, Row row)
{
    // Both apply when the next track uploads its textures.
    return row == Row::mipmaps || row == Row::texture_edge_fix || row == Row::high_precision_textures ||
                   row == Row::sharp_hud
               ? l.mipmaps_note
               : std::u8string_view{};
}

// ---------------------------------------------------------------- Latin-1
Latin1Text::Latin1Text(std::span<std::uint8_t> out) : out_(out)
{
    if (out_.empty())
        throw std::invalid_argument("GRAPHICS text needs a buffer");
}
Latin1Text &Latin1Text::append(std::u8string_view text)
{
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        auto c = static_cast<std::uint32_t>(text[i]);
        if (c >= 0x80u)
        {
            if ((c == 0xc2u || c == 0xc3u) && i + 1 < text.size() &&
                (static_cast<std::uint32_t>(text[i + 1]) & 0xc0u) == 0x80u)
                c = ((c & 0x1fu) << 6) | (static_cast<std::uint32_t>(text[++i]) & 0x3fu);
            else
                throw std::invalid_argument("host GRAPHICS text is outside the Latin-1 glyph set");
        }
        if (c == 0 || size_ + 1 >= out_.size())
            throw std::length_error("host GRAPHICS text exceeds its buffer");
        out_[size_++] = static_cast<std::uint8_t>(c);
    }
    return *this;
}
Latin1Text &Latin1Text::append_value(const Language &l, Row row, const Settings &s)
{
    const auto flag = [&](bool on) -> Latin1Text & { return append(on ? l.on : l.off); };
    const auto number = [&](std::uint32_t value) -> Latin1Text & {
        // R30: decimal digits without a std::string per value (the rows are composed every menu frame).
        char digits[16];
        const auto end = std::to_chars(digits, digits + sizeof digits, value).ptr;
        return append(std::u8string_view(reinterpret_cast<const char8_t *>(digits), std::size_t(end - digits)));
    };
    switch (row)
    {
    case Row::display_mode: {
        const auto mode = static_cast<std::size_t>(s.display_mode);
        if (mode >= l.display_modes.size())
            throw std::logic_error("host display mode outside its sanitized domain");
        return append(l.display_modes[mode]);
    }
    case Row::interpolation:
        return flag(s.interpolation);
    case Row::vsync:
        return flag(s.vsync);
    case Row::aspect:
        return append(s.original_4x3 ? l.original_4x3 : l.widescreen);
    case Row::anti_aliasing:
        if (s.msaa_samples <= 1u)
            return append(l.off);
        number(s.msaa_samples);
        return append(u8"X MSAA");
    case Row::texture_filter:
        switch (s.texture_filter)
        {
        case TextureFilter::original:
            return append(l.original);
        case TextureFilter::trilinear:
            return append(l.trilinear);
        case TextureFilter::anisotropic:
            append(l.anisotropic);
            append(u8" ");
            number(s.anisotropy);
            return append(u8"X");
        }
        break;
    case Row::mipmaps:
        return flag(s.generate_mipmaps);
    case Row::alpha_to_coverage:
        return flag(s.alpha_to_coverage);
    case Row::fxaa:
        return flag(s.post_anti_aliasing == PostAntiAliasing::fxaa);
    case Row::smooth_gradients:
        return flag(s.smooth_gradients);
    case Row::texture_edge_fix:
        return flag(s.texture_edge_fix);
    case Row::frame_rate_limit:
        if (s.frame_rate_limit == 0)
            return append(l.off);
        number(s.frame_rate_limit);
        return append(u8" FPS");
    case Row::low_latency:
        return flag(s.low_latency);
    case Row::high_precision_textures:
        return flag(s.high_precision_textures);
    case Row::sharp_hud:
        return flag(s.sharp_hud);
    case Row::texture_sharpness:
        if (s.texture_sharpness > 0)
            append(u8"+");
        else if (s.texture_sharpness < 0)
            append(u8"-");
        return number(static_cast<std::uint32_t>(s.texture_sharpness < 0 ? -s.texture_sharpness : s.texture_sharpness));
    case Row::smooth_magnification:
        return flag(s.smooth_magnification);
    case Row::bloom:
        return flag(s.bloom);
    case Row::smooth_effects:
        return flag(s.smooth_effects);
    case Row::reset:
        break;
    }
    throw std::logic_error("host GRAPHICS row has no value");
}
Latin1Text &Latin1Text::append_row(const Language &l, Row row, const Settings &s)
{
    append(label(l, row));
    if (has_value(row))
    {
        append(l.separator);
        append_value(l, row, s);
    }
    return *this;
}
std::span<std::uint8_t> Latin1Text::finish()
{
    out_[size_] = 0;
    return out_.first(size_ + 1);
}
} // namespace sgn::host_graphics_menu
