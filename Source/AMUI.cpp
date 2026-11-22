#include "AMUI.h"
#include <map>

namespace AMUI {
namespace {
// White strokes/fills; recoloured when drawn. Stroke attributes are on the root <g>.
const char* svgBody(Icon icon) {
    switch (icon) {
        case Icon::play: return R"(<path d="M7 4.5v15a1 1 0 0 0 1.5.86l12.2-7.5a1 1 0 0 0 0-1.72L8.5 3.64A1 1 0 0 0 7 4.5z" fill="#ffffff" stroke="none"/>)";
        case Icon::stop: return R"(<rect x="6" y="6" width="12" height="12" rx="2" fill="#ffffff" stroke="none"/>)";
        case Icon::variation: return R"(<path d="M21 12a9 9 0 1 1-2.64-6.36L21 8"/><path d="M21 3v5h-5"/>)";
        case Icon::dice: return R"(<rect x="3" y="3" width="18" height="18" rx="4"/><circle cx="8.5" cy="8.5" r="1.3" fill="#ffffff" stroke="none"/><circle cx="15.5" cy="15.5" r="1.3" fill="#ffffff" stroke="none"/><circle cx="12" cy="12" r="1.3" fill="#ffffff" stroke="none"/><circle cx="15.5" cy="8.5" r="1.3" fill="#ffffff" stroke="none"/><circle cx="8.5" cy="15.5" r="1.3" fill="#ffffff" stroke="none"/>)";
        case Icon::newMelody: return R"(<path d="M9 18V5l11-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="17" cy="16" r="3"/>)";
        case Icon::sparkle: return R"(<path d="M10.5 3.5l1.7 5.1a2 2 0 0 0 1.2 1.2l5.1 1.7-5.1 1.7a2 2 0 0 0-1.2 1.2l-1.7 5.1-1.7-5.1a2 2 0 0 0-1.2-1.2L2.5 11.5l5.1-1.7a2 2 0 0 0 1.2-1.2z" fill="#ffffff"/><path d="M19 3v4M21 5h-4"/>)";
        case Icon::history: return R"(<path d="M3 12a9 9 0 1 0 2.64-6.36L3 8"/><path d="M3 3v5h5"/><path d="M12 7v5l3.5 2"/>)";
        case Icon::midi: return R"(<path d="M14 2.5H7a2 2 0 0 0-2 2v15a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-12z"/><path d="M14 2.5v5h5"/><circle cx="10.5" cy="16.5" r="1.8"/><path d="M12.3 16.5v-5.5l2.7 1"/>)";
        case Icon::wav: return R"(<path d="M14 2.5H7a2 2 0 0 0-2 2v15a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-12z"/><path d="M14 2.5v5h5"/><path d="M8.5 15v1M10.75 12.5v6M13 14v3M15.25 13v5"/>)";
        case Icon::save: return R"(<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><path d="M7 10l5 5 5-5"/><path d="M12 15V3"/>)";
        case Icon::gear: return R"(<path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"/><circle cx="12" cy="12" r="3"/>)";
        case Icon::lock: return R"(<rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>)";
        case Icon::unlock: return R"(<rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 7.8-1.2"/>)";
        case Icon::record: return R"(<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="3.5" fill="#ffffff" stroke="none"/>)";
        case Icon::key: return R"(<circle cx="8" cy="15" r="4.5"/><path d="M11.2 11.8L20 3M17 6l3 3M15 8l2 2"/>)";
        case Icon::mail: return R"(<rect x="2.5" y="4.5" width="19" height="15" rx="2"/><path d="M3 6.5l9 6 9-6"/>)";
        case Icon::drum: return R"(<ellipse cx="12" cy="8" rx="9" ry="3.5"/><path d="M3 8v8c0 1.9 4 3.5 9 3.5s9-1.6 9-3.5V8"/><path d="M7 11.2v8M12 11.5v8M17 11.2v8"/>)";
        case Icon::piano: return R"(<rect x="2.5" y="4.5" width="19" height="15" rx="2"/><path d="M8.8 4.5v15M15.2 4.5v15"/><path d="M6.6 4.5v7M11 4.5v7M13 4.5v7M17.4 4.5v7" stroke-width="2.6"/>)";
        case Icon::chevron: return R"(<path d="M6 9l6 6 6-6"/>)";
        case Icon::left: return R"(<path d="M15 18l-6-6 6-6"/>)";
        case Icon::right: return R"(<path d="M9 18l6-6-6-6"/>)";
        case Icon::plus: return R"(<path d="M12 5v14M5 12h14"/>)";
        case Icon::close: return R"(<path d="M18 6L6 18M6 6l12 12"/>)";
        case Icon::check: return R"(<path d="M20 6L9 17l-5-5"/>)";
        case Icon::note: return R"(<path d="M9 18V5l11-2v13" stroke-width="2.4"/><circle cx="6" cy="18" r="3" fill="#ffffff"/><circle cx="17" cy="16" r="3" fill="#ffffff"/>)";
        case Icon::bars: return R"(<path d="M5 9v6M9.5 5v14M14.5 7v10M19 10v4" stroke-width="2.6"/>)";
        case Icon::sub: return R"(<path d="M2 12c2.5-6 5.5-6 8 0s5.5 6 8 0 2.5-3 4-3"/><path d="M2 20h20"/>)";
        case Icon::none: break;
    }
    return "";
}

const juce::Drawable* drawableFor(Icon icon) {
    static std::map<Icon, std::unique_ptr<juce::Drawable>> cache;
    if (auto it = cache.find(icon); it != cache.end()) return it->second.get();
    const juce::String svg = juce::String("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
                                          "<g fill=\"none\" stroke=\"#ffffff\" stroke-width=\"1.75\" stroke-linecap=\"round\" stroke-linejoin=\"round\">")
                           + svgBody(icon) + "</g></svg>";
    std::unique_ptr<juce::Drawable> d;
    if (auto xml = juce::XmlDocument::parse(svg)) d = juce::Drawable::createFromSVG(*xml);
    return (cache[icon] = std::move(d)).get();
}
}

void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c) {
    if (icon == Icon::none) return;
    const auto* base = drawableFor(icon);
    if (base == nullptr) return;
    auto copy = base->createCopy();
    copy->replaceColour(juce::Colours::white, c);
    // the SVG keeps its 24-unit grid, so every icon has the same visual weight at the same size
    const float s = juce::jmin(r.getWidth(), r.getHeight());
    const auto box = juce::Rectangle<float>(s, s).withCentre(r.getCentre());
    copy->draw(g, 1.0f, juce::AffineTransform::scale(s / 24.0f).translated(box.getX(), box.getY()));
}

namespace {
// towlower ignores accents in the C locale, so Latin-1 capitals (Ã, É, Ç...) are folded by hand.
juce::String lower(const juce::String& w) {
    juce::String out;
    for (auto c : w) out += (juce::juce_wchar) ((c >= 0xC0 && c <= 0xDE && c != 0xD7) ? c + 0x20 : juce::CharacterFunctions::toLowerCase(c));
    return out;
}
juce::String upperFirst(const juce::String& w) {
    if (w.isEmpty()) return w;
    auto c = w[0];
    c = (c >= 0xE0 && c <= 0xFE && c != 0xF7) ? c - 0x20 : juce::CharacterFunctions::toUpperCase(c);
    return juce::String::charToString(c) + w.substring(1);
}
}

juce::String displayCase(const juce::String& text) {
    static const juce::StringArray keep { "R&B", "UK", "BR", "EDM", "BPM", "MIDI", "WAV", "FM", "808", "303", "DJ", "LP", "TR", "II", "III", "IV", "VI", "VII" };
    juce::StringArray words;
    words.addTokens(text, " ", "");
    for (int i = 0; i < words.size(); ++i) {
        auto& w = words.getReference(i);
        if (keep.contains(w)) continue;
        // note names (C, C#, Db...) stay as written
        if (w.length() <= 2 && juce::String("ABCDEFG").containsChar(w[0]) && (w.length() == 1 || w[1] == '#' || w[1] == 'b')) continue;
        w = lower(w);
        if (i == 0 || words[i - 1] == juce::String::fromUTF8("\xe2\x86\x92")) w = upperFirst(w);
    }
    return words.joinIntoString(" ");
}
}
