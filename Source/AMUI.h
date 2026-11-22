#pragma once
// Audio Makers UI: pieces shared by Chord Code and Melody Code (same file in both repos, keep them in sync).
// One icon set (24-unit grid, 1.75 stroke, rounded caps, drawn in the style of Lucide, ISC licence) and the
// text rules of the design system.
#include <juce_gui_basics/juce_gui_basics.h>

namespace AMUI {

enum class Icon {
    none, play, stop, variation, dice, newMelody, sparkle, history, midi, wav, save, gear, lock, unlock, record,
    key, mail, drum, piano, chevron, left, right, plus, close, note, bars, check, sub
};

// Draws the icon centred in r (square), in one colour.
void drawIcon(juce::Graphics&, Icon, juce::Rectangle<float> r, juce::Colour);

// "MENOR NATURAL" -> "Menor natural": values are written in normal case; acronyms and note names stay as they are.
juce::String displayCase(const juce::String&);

// Spacing and size tokens (4-point grid).
namespace Size {
constexpr int buttonSmall = 32, button = 40, buttonLarge = 52, field = 44, icon = 20, iconSmall = 16;
constexpr float radiusTag = 6.0f, radiusControl = 10.0f, radiusPanel = 14.0f, radiusWindow = 18.0f;
}

// Letter spacing (JUCE kerning factor) for the two upper-case styles.
constexpr float kerningLabel = 0.08f;    // 11 px overline labels
constexpr float kerningButton = 0.04f;   // 12-13 px button text
constexpr float kerningHero = 0.08f;     // the one hero button (GERAR)
}
