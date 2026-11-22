#include "PluginEditor.h"
#include "EmbeddedFonts.h"

namespace {
const char* noteNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* roman[] { "I", "II", "III", "IV", "V", "VI", "VII" };
juce::String utf8(const char* s) { return juce::String::fromUTF8(s); }

// ---- files for SALVAR / ARRASTAR (same fixes as Chord Code on Windows) ----------------------
// Everything goes through juce::File (wide Win32 API), so an accented user folder (C:\Users\João)
// works. A write is checked on disk afterwards, because antivirus or "Controlled folder access"
// can drop it silently.
bool isPlainAscii(const juce::String& text) {
    for (auto c : text) if (c > 127) return false;
    return true;
}

bool writeBytes(const juce::File& file, const void* data, size_t size, juce::String& error) {
    if (const auto made = file.getParentDirectory().createDirectory(); made.failed()) { error = made.getErrorMessage(); return false; }
    if (!file.replaceWithData(data, size)) {
        // replaceWithData writes a temp file and renames it; some antivirus / OneDrive setups block the rename
        juce::FileOutputStream out(file);
        if (!out.openedOk()) { error = out.getStatus().failed() ? out.getStatus().getErrorMessage() : juce::String::fromUTF8("sem permiss\xc3\xa3o de escrita"); return false; }
        out.setPosition(0); out.truncate(); out.write(data, size); out.flush();
        if (out.getStatus().failed()) { error = out.getStatus().getErrorMessage(); return false; }
    }
    if (!file.existsAsFile() || file.getSize() != (juce::int64) size) {
        error = juce::String::fromUTF8("o arquivo n\xc3\xa3o ficou no disco (antiv\xc3\xadrus ou Acesso Controlado a Pastas?)");
        return false;
    }
    return true;
}

// Where drag files are written, best first: plain-ASCII folders first (a few hosts mishandle dropped paths
// with accents), protected folders (Documents, Music) last (Windows "Controlled folder access" blocks DAWs).
juce::Array<juce::File> dragFolders() {
    const auto sub = [](const juce::File& base) { return base.getChildFile("Audio Makers").getChildFile("Melody Code"); };
    juce::Array<juce::File> candidates;
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::tempDirectory)));
   #if JUCE_WINDOWS
    if (const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {}); local.isNotEmpty()) candidates.add(sub(juce::File(local)).getChildFile("Arrastar"));
    if (const auto pub = juce::SystemStats::getEnvironmentVariable("PUBLIC", {}); pub.isNotEmpty()) candidates.add(sub(juce::File(pub)).getChildFile("Arrastar"));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)).getChildFile("Arrastar"));
   #endif
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)).getChildFile("Arrastar"));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)));
    candidates.add(sub(juce::File::getSpecialLocation(juce::File::userMusicDirectory)));
    juce::Array<juce::File> ascii, other;
    for (const auto& dir : candidates) {
        if (dir.getFullPathName().length() < 4 || ascii.contains(dir) || other.contains(dir)) continue;
        (isPlainAscii(dir.getFullPathName()) ? ascii : other).add(dir);
    }
    ascii.addArray(other);
    return ascii;
}
bool isBlack(int m) { const int pc = m % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
}

namespace Theme {
juce::Font font(float h, bool bold) {
    static auto regular = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interRegular, EmbeddedFonts::interRegularSize);
    static auto heavy = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interBold, EmbeddedFonts::interBoldSize);
    return juce::Font(juce::FontOptions(bold ? heavy : regular).withHeight(h));
}
juce::Font semi(float h) {
    static auto face = juce::Typeface::createSystemTypefaceFor(EmbeddedFonts::interSemiBold, EmbeddedFonts::interSemiBoldSize);
    return juce::Font(juce::FontOptions(face).withHeight(h));
}
juce::Font label(float h) { return semi(h).withExtraKerningFactor(AMUI::kerningLabel); }
juce::Font button(float h) { return semi(h).withExtraKerningFactor(AMUI::kerningButton); }
juce::ColourGradient accent(juce::Rectangle<float> r) {
    return juce::ColourGradient(acc, r.getX(), r.getY(), acc2, r.getRight(), r.getBottom(), false);
}
}

// ---------------------------------------------------------------- look and feel
MelodyLook::MelodyLook() {
    using namespace Theme;
    setColour(juce::ComboBox::textColourId, fg);
    setColour(juce::ComboBox::backgroundColourId, panel2);
    setColour(juce::ComboBox::outlineColourId, line);
    setColour(juce::ComboBox::arrowColourId, mute);
    setColour(juce::PopupMenu::backgroundColourId, panel2);
    setColour(juce::PopupMenu::textColourId, fg);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, acc.withAlpha(0.18f));
    setColour(juce::PopupMenu::highlightedTextColourId, acc);
    setColour(juce::PopupMenu::headerTextColourId, mute);
    setColour(juce::Slider::textBoxTextColourId, fg);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
}

void MelodyLook::drawComboBox(juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) {
    using namespace Theme;
    auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h).reduced(0.5f);
    const bool small = h < 34, over = box.isMouseOver(true), open = box.isPopupActive() || down;
    const float radius = small ? 8.0f : AMUI::Size::radiusControl;
    g.setColour(small ? bg : (over ? hover : panel2));
    g.fillRoundedRectangle(r, radius);
    g.setColour(open ? acc : over || box.hasKeyboardFocus(false) ? acc.withAlpha(0.6f) : line);
    g.drawRoundedRectangle(r, radius, 1.0f);
    const float s = small ? 14.0f : 16.0f;
    drawIcon(g, Icon::chevron, { (float) w - (small ? 8.0f : 14.0f) - s, ((float) h - s) * 0.5f, s, s }, open || over ? acc : mute);
}

void MelodyLook::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    const bool small = box.getHeight() < 34;
    label.setBounds(small ? 4 : 8, 1, box.getWidth() - (small ? 24 : 40), box.getHeight() - 2);
    label.setFont(small ? Theme::semi(13.0f) : Theme::semi(15.0f));
}

void MelodyLook::drawPopupMenuBackground(juce::Graphics& g, int w, int h) {
    g.fillAll(Theme::panel2);
    g.setColour(Theme::line2);
    g.drawRect(0, 0, w, h);
}

void MelodyLook::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                  juce::Slider::SliderStyle, juce::Slider&) {
    const float cy = (float) y + (float) h * 0.5f;
    auto track = juce::Rectangle<float>((float) x, cy - 2, (float) w, 4);
    g.setColour(Theme::line2);
    g.fillRoundedRectangle(track, 2);
    g.setGradientFill(Theme::accent(track));
    g.fillRoundedRectangle(track.withRight(pos), 2);
    g.setColour(Theme::acc.withAlpha(0.35f));
    g.fillEllipse(pos - 11, cy - 11, 22, 22);
    g.setColour(Theme::fg);
    g.fillEllipse(pos - 8, cy - 8, 16, 16);
}

// ---------------------------------------------------------------- small widgets
Segmented::Segmented(std::initializer_list<const char*> o) {
    for (auto* x : o) options.add(juce::String::fromUTF8(x));
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

// Hover explanations: a small dark card with the accent border, wrapped to a readable width.
namespace { juce::TextLayout tooltipLayout(const juce::String& text) {
    juce::AttributedString a; a.setJustification(juce::Justification::centredLeft);
    a.append(text, Theme::font(13.0f), Theme::fg);
    juce::TextLayout l; l.createLayoutWithBalancedLineLengths(a, 300.0f);
    return l;
} }

juce::Rectangle<int> MelodyLook::getTooltipBounds(const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) {
    const auto l = tooltipLayout(text);
    const int w = (int) l.getWidth() + 28, h = (int) l.getHeight() + 20;
    return juce::Rectangle<int>(pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12, pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin(parent);
}

void MelodyLook::drawTooltip(juce::Graphics& g, const juce::String& text, int w, int h) {
    const auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h);
    g.setColour(Theme::panel2); g.fillRoundedRectangle(r, 8);
    g.setColour(Theme::line2); g.drawRoundedRectangle(r.reduced(0.5f), 8, 1);
    tooltipLayout(text).draw(g, r.reduced(14, 10));
}

void Segmented::paint(juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour(Theme::bg);
    g.fillRoundedRectangle(r, AMUI::Size::radiusControl);
    g.setColour(Theme::line);
    g.drawRoundedRectangle(r.reduced(0.5f), AMUI::Size::radiusControl, 1.0f);
    const float w = (r.getWidth() - 6) / (float) options.size();
    for (int i = 0; i < options.size(); ++i) {
        auto c = juce::Rectangle<float>(3 + w * (float) i, 3, w, r.getHeight() - 6);
        if (i == selected) {
            g.setGradientFill(Theme::accent(c));
            g.fillRoundedRectangle(c, 7);
        }
        g.setColour(i == selected ? Theme::ink : Theme::mute);
        g.setFont(Theme::button(12.0f));
        g.drawText(options[i], c, juce::Justification::centred);
    }
}

void Segmented::mouseDown(const juce::MouseEvent& e) {
    const int i = juce::jlimit(0, options.size() - 1, (int) ((float) e.x / (float) getWidth() * (float) options.size()));
    if (i != selected) { selected = i; repaint(); if (onChange) onChange(i); }
}

// Audio Makers UI buttons: same corner (10) and the same 13 px SemiBold label for every size; only GERAR is the hero.
void ActionButton::paintButton(juce::Graphics& g, bool over, bool down) {
    using namespace Theme;
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    const float radius = AMUI::Size::radiusControl;
    const bool enabled = isEnabled();
    juce::Colour textColour, iconColour;
    if (isAccent) {
        juce::Path p; p.addRoundedRectangle(r, radius);
        if (enabled) juce::DropShadow(acc.withAlpha(over ? 0.38f : 0.24f), hero ? 22 : 14, { 0, 4 }).drawForPath(g, p);
        g.setGradientFill(accent(r));
        g.fillPath(p);
        if (over) { g.setColour(juce::Colours::white.withAlpha(0.08f)); g.fillPath(p); }
        if (down || !enabled) { g.setColour(juce::Colours::black.withAlpha(0.18f)); g.fillPath(p); }
        textColour = iconColour = ink;
    } else {
        g.setColour(down ? panel : over ? hover : panel2);
        g.fillRoundedRectangle(r, radius);
        g.setColour(lit ? acc : over ? acc.withAlpha(0.6f) : line);
        g.drawRoundedRectangle(r, radius, lit ? 1.5f : 1.0f);
        textColour = lit ? acc : fg;
        iconColour = lit || over ? acc : mute;
    }
    juce::Font f = hero ? semi(15.0f).withExtraKerningFactor(AMUI::kerningHero) : button(13.0f);
    const float iconSize = (float) AMUI::Size::icon, gap = 8.0f;
    const auto text = getButtonText();
    float tw = juce::GlyphArrangement::getStringWidth(f, text);
    const float room = r.getWidth() - 24.0f - (icon == Icon::none ? 0.0f : iconSize + gap);
    if (tw > room) { f = button(12.0f); tw = juce::GlyphArrangement::getStringWidth(f, text); }
    g.setFont(f);
    if (icon == Icon::none || tw > room) {
        g.setColour(textColour);
        g.drawFittedText(text, getLocalBounds().reduced(8, 0), juce::Justification::centred, 1, 0.9f);
        return;
    }
    const float total = iconSize + gap + tw;
    const float x0 = std::round(r.getCentreX() - total / 2);
    drawIcon(g, icon, { x0, r.getCentreY() - iconSize / 2, iconSize, iconSize }, iconColour);
    g.setColour(textColour);
    g.drawText(text, juce::Rectangle<float>(x0 + iconSize + gap, r.getY(), tw + 4, r.getHeight()), juce::Justification::centredLeft);
}

void IconButton::paintButton(juce::Graphics& g, bool over, bool down) {
    using namespace Theme;
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    if (ghost) {
        if (over || down) { g.setColour(down ? line : line.withAlpha(0.7f)); g.fillRoundedRectangle(r, 8.0f); }
        drawIcon(g, icon, juce::Rectangle<float>(16, 16).withCentre(r.getCentre()), over ? acc : mute);
        return;
    }
    g.setColour(down ? panel : over ? hover : panel2);
    g.fillRoundedRectangle(r, AMUI::Size::radiusControl);
    g.setColour(over ? acc.withAlpha(0.6f) : line);
    g.drawRoundedRectangle(r, AMUI::Size::radiusControl, 1.0f);
    drawIcon(g, icon, juce::Rectangle<float>(20, 20).withCentre(r.getCentre()), over ? acc : mute);
    if (dot) {
        const auto c = juce::Point<float>(r.getRight() - 2, r.getY() + 2);
        g.setColour(bg); g.fillEllipse(juce::Rectangle<float>(14, 14).withCentre(c));
        g.setColour(warn); g.fillEllipse(juce::Rectangle<float>(9, 9).withCentre(c));
    }
}

// ---------------------------------------------------------------------------

void ModalOverlay::present(juce::Component& owner, std::unique_ptr<juce::Component> panel, float scale, bool lock) {
    const bool was = isPresenting();
    locked = lock;
    if (content) removeChildComponent(content.get());
    content = std::move(panel);
    uiScale = scale;
    if (!was) {
        setVisible(false);
        // blur: shrink the snapshot, soften it, stretch it back when drawn
        const auto snap = owner.createComponentSnapshot(owner.getLocalBounds(), true, 1.0f);
        const int w = juce::jmax(1, snap.getWidth() / 6), h = juce::jmax(1, snap.getHeight() / 6);
        juce::Image small(juce::Image::ARGB, w, h, true);
        {
            juce::Graphics g(small);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(snap, 0, 0, w, h, 0, 0, snap.getWidth(), snap.getHeight());
        }
        juce::ImageConvolutionKernel kernel(7);
        kernel.createGaussianBlur(2.2f);
        kernel.applyToImage(small, small, small.getBounds());
        backdrop = small;
        reveal = 0;
    }
    setBounds(owner.getLocalBounds());
    addAndMakeVisible(*content);
    layoutPanel();
    setVisible(true);
    toFront(true);
    if (!was) startTimerHz(60);
}

void ModalOverlay::dismiss() {
    if (!content) return;
    stopTimer();
    removeChildComponent(content.get());
    juce::MessageManager::callAsync([old = std::shared_ptr<juce::Component>(content.release())] {});
    backdrop = {};
    locked = false;
    setVisible(false);
}

void ModalOverlay::rescale(juce::Rectangle<int> bounds, float scale) {
    if (!content) return;
    uiScale = scale;
    setBounds(bounds);
    layoutPanel();
    repaint();
}

void ModalOverlay::layoutPanel() {
    if (!content) return;
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    const float w = (float) content->getWidth() * uiScale, h = (float) content->getHeight() * uiScale;
    const float x = ((float) getWidth() - w) * 0.5f, y = ((float) getHeight() - h) * 0.5f + (1.0f - ease) * 14.0f;
    content->setTopLeftPosition(0, 0);
    content->setTransform(juce::AffineTransform::scale(uiScale).translated(x, y));
    content->setAlpha(ease);
}

void ModalOverlay::timerCallback() {
    reveal = juce::jmin(1.0f, reveal + 1.0f / 9.0f);
    layoutPanel();
    repaint();
    if (reveal >= 1.0f) stopTimer();
}

void ModalOverlay::paint(juce::Graphics& g) {
    const float ease = 1.0f - std::pow(1.0f - reveal, 3.0f);
    if (backdrop.isValid()) {
        g.setOpacity(ease);
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(backdrop, getLocalBounds().toFloat());
        g.setOpacity(1.0f);
    }
    g.setColour(juce::Colour(0xff040506).withAlpha(0.58f * ease));
    g.fillAll();
    if (content) {
        juce::Path shape;
        shape.addRoundedRectangle(content->getBounds().toFloat().transformedBy(content->getTransform()), 16.0f * uiScale);
        juce::DropShadow(juce::Colours::black.withAlpha(0.6f * ease), juce::roundToInt(40.0f * uiScale), { 0, juce::roundToInt(14.0f * uiScale) }).drawForPath(g, shape);
    }
}

void ModalOverlay::mouseUp(const juce::MouseEvent& e) {
    if (content && !locked && !content->getBounds().toFloat().transformedBy(content->getTransform()).contains(e.position)) dismiss();
}

bool ModalOverlay::keyPressed(const juce::KeyPress& k) {
    if (k == juce::KeyPress::escapeKey) { if (!locked) dismiss(); return true; }
    return false;
}

// ---------------------------------------------------------------------------

SettingsPanel::SettingsPanel(MelodyCodeProcessor& p, UpdateStatus& u, std::function<void()> c, std::function<void()> ch, std::function<void()> cu)
    : proc(p), update(u), close(std::move(c)), changed(std::move(ch)), checkUpdate(std::move(cu)) {
    setSize(panelWidth, 784);
    startTimerHz(4);   // keeps the DAW tempo readout fresh
}

void SettingsPanel::paint(juce::Graphics& g) {
    using namespace Theme;
    auto all = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(all, AMUI::Size::radiusWindow);
    g.setGradientFill(juce::ColourGradient(acc.withAlpha(0.10f), 0, 0, acc.withAlpha(0.0f), 0, 90, false));
    g.fillRoundedRectangle(all, AMUI::Size::radiusWindow);
    g.setColour(line);
    g.drawRoundedRectangle(all.reduced(0.5f), AMUI::Size::radiusWindow, 1);

    auto section = [&](const juce::String& t, float y) {
        g.setColour(mute); g.setFont(label());
        g.drawText(t, juce::Rectangle<float>(18, y, 260, 16), juce::Justification::centredLeft);
    };
    auto divider = [&](float y) { g.setColour(line); g.fillRect(18.0f, y, panelWidth - 36.0f, 1.0f); };
    auto toggle = [&](juce::Rectangle<float> row, const juce::String& title, bool on, const juce::String& note) {
        g.setColour(isOver(row) ? hover : panel2);
        g.fillRoundedRectangle(row, 10);
        g.setColour(fg); g.setFont(semi(14.0f));
        g.drawText(title, row.withTrimmedLeft(12), juce::Justification::centredLeft);
        auto track = juce::Rectangle<float>(40, 22).withCentre({ row.getRight() - 32, row.getCentreY() });
        if (on) g.setGradientFill(accent(track)); else g.setColour(line2);
        g.fillRoundedRectangle(track, 11);
        g.setColour(fg);
        g.fillEllipse(juce::Rectangle<float>(16, 16).withCentre({ on ? track.getRight() - 11 : track.getX() + 11, track.getCentreY() }));
        if (note.isNotEmpty()) { g.setColour(mute); g.setFont(font(12.5f)); g.drawText(note, row.withTrimmedRight(62), juce::Justification::centredRight); }
    };

    auto mark = juce::Rectangle<float>(18, 18, 34, 34);
    g.setColour(acc.withAlpha(0.12f)); g.fillRoundedRectangle(mark, 9);
    g.setColour(acc.withAlpha(0.35f)); g.drawRoundedRectangle(mark.reduced(0.5f), 9, 1);
    drawIcon(g, Icon::gear, juce::Rectangle<float>(18, 18).withCentre(mark.getCentre()), acc);
    g.setColour(mute); g.setFont(label());
    g.drawText(juce::String::fromUTF8("CONFIGURAÇÕES"), 62, 17, 200, 16, juce::Justification::centredLeft);
    g.setColour(fg); g.setFont(font(18.0f, true));
    g.drawText("Melody Code", 62, 32, 140, 24, juce::Justification::centredLeft);
    auto ver = juce::Rectangle<float>(190, 34, 62, 22);
    g.setColour(acc.withAlpha(0.12f)); g.fillRoundedRectangle(ver, 11);
    g.setColour(acc.withAlpha(0.4f)); g.drawRoundedRectangle(ver.reduced(0.5f), 11, 1);
    g.setColour(acc); g.setFont(semi(12.5f));
    g.drawText("v" JucePlugin_VersionString, ver, juce::Justification::centred);
    {
        const auto c = closeArea();
        if (isOver(c)) { g.setColour(line); g.fillRoundedRectangle(c, 8); }
        drawIcon(g, Icon::close, juce::Rectangle<float>(16, 16).withCentre(c.getCentre()), isOver(c) ? fg : mute);
    }
    divider(70);

    section("TEMPO", 80);
    const bool synced = proc.syncToHost.load();
    toggle(syncRow(), "Sincronizar com a DAW", synced, synced && proc.hostIsPlaying() ? juce::String::fromUTF8("tocando junto") : juce::String());
    for (size_t i = 0; i < bpmValues.size(); ++i) {
        const auto r = chip(i);
        const bool active = proc.bpm == bpmValues[i];
        if (active) { g.setGradientFill(accent(r)); g.fillRoundedRectangle(r, 8); }
        else {
            g.setColour(isOver(r) ? hover : panel2); g.fillRoundedRectangle(r, 8);
            g.setColour(isOver(r) ? acc.withAlpha(0.7f) : line); g.drawRoundedRectangle(r.reduced(0.5f), 8, 1);
        }
        g.setColour(active ? ink : fg); g.setFont(semi(14.0f));
        g.drawText(juce::String(bpmValues[i]), r, juce::Justification::centred);
    }
    g.setColour(quiet.brighter(0.4f)); g.setFont(font(12.0f));
    g.drawFittedText(juce::String::fromUTF8("Com a DAW tocando, o loop segue o tempo e o compasso dela. Parada, usa o BPM acima."),
                     juce::Rectangle<int>(18, 224, panelWidth - 36, 32), juce::Justification::centredLeft, 2, 1.0f);
    divider(258);

    section("MIDI", 266);
    toggle(midiRow(), "Enviar MIDI para a DAW", proc.midiOut.load(), {});
    g.setColour(quiet.brighter(0.4f)); g.setFont(font(12.0f));
    g.drawFittedText(juce::String::fromUTF8("Melodia no canal 1, grave no 2 e acordes no 3, pra gravar ou tocar com outro instrumento."),
                     juce::Rectangle<int>(18, 334, panelWidth - 36, 32), juce::Justification::centredLeft, 2, 1.0f);
    divider(372);

    section("ARRASTAR", 380);
    toggle(dragRow(), "Incluir acordes e grave", proc.dragAllLayers.load(), {});
    g.setColour(quiet.brighter(0.4f)); g.setFont(font(12.0f));
    g.drawFittedText(juce::String::fromUTF8("Desligado, os botões MIDI e WAV levam só a melodia. Ligado, levam melodia, acordes e grave."),
                     juce::Rectangle<int>(18, 448, panelWidth - 36, 32), juce::Justification::centredLeft, 2, 1.0f);
    divider(486);

    section("JANELA", 494);
    {
        const float current = windowScale ? windowScale() : 1.0f;
        for (size_t i = 0; i < scaleValues.size(); ++i) {
            const auto r = scaleChip(i);
            const bool active = std::abs(current - scaleValues[i]) < 0.01f;
            if (active) { g.setGradientFill(accent(r)); g.fillRoundedRectangle(r, 8); }
            else {
                g.setColour(isOver(r) ? hover : panel2); g.fillRoundedRectangle(r, 8);
                g.setColour(isOver(r) ? acc.withAlpha(0.7f) : line); g.drawRoundedRectangle(r.reduced(0.5f), 8, 1);
            }
            g.setColour(active ? ink : fg); g.setFont(semi(14.0f));
            g.drawText(juce::String(juce::roundToInt(scaleValues[i] * 100)) + "%", r, juce::Justification::centred);
        }
        g.setColour(mute); g.setFont(font(12.0f));
        g.drawText(juce::String::fromUTF8("Tamanho padrão ao abrir o plugin"), juce::Rectangle<float>(18, 554, panelWidth - 36.0f, 16), juce::Justification::centredLeft);
    }
    divider(576);

    section(juce::String::fromUTF8("LICENÇA"), 582);
    {
        const auto row = licenseRow();
        g.setColour(isOver(row) ? hover : panel2);
        g.fillRoundedRectangle(row, 10);
        g.setColour(fg); g.setFont(semi(13.5f));
        const auto mail = licenseEmail ? licenseEmail() : juce::String();
        g.drawFittedText(mail.isNotEmpty() ? mail : juce::String::fromUTF8("Não ativada"), row.withTrimmedLeft(12).withTrimmedRight(80).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setColour(isOver(row) ? acc : mute); g.setFont(semi(12.5f));
        g.drawText("Trocar", row.withTrimmedRight(14), juce::Justification::centredRight);
    }
    divider(656);

    section(juce::String::fromUTF8("ATUALIZAÇÃO"), 664);
    {
        const auto row = updateRow();
        g.setColour(isOver(row) ? hover : panel2);
        g.fillRoundedRectangle(row, 10);
        const bool avail = update.state == UpdateStatus::available;
        g.setColour(fg); g.setFont(semi(14.0f));
        g.drawText(avail ? juce::String::fromUTF8("Baixar atualização") : juce::String::fromUTF8("Verificar atualização"), row.withTrimmedLeft(12), juce::Justification::centredLeft);
        juce::String note;
        switch (update.state) {
            case UpdateStatus::checking:  note = "verificando..."; break;
            case UpdateStatus::upToDate:  note = juce::String::fromUTF8("você está em dia"); break;
            case UpdateStatus::available: note = "v" + update.latest + juce::String::fromUTF8(" disponível"); break;
            case UpdateStatus::failed:    note = juce::String::fromUTF8("sem conexão"); break;
            case UpdateStatus::idle:      break;
        }
        g.setColour(avail ? warn : mute);
        g.setFont(font(12.5f));
        g.drawText(note, row.withTrimmedRight(14), juce::Justification::centredRight);
    }
    divider(740);
    g.setColour(mute); g.setFont(font(12.0f));
    g.drawText("Audio Makers", juce::Rectangle<float>(18, 752, 200, 20), juce::Justification::centredLeft);
}

void SettingsPanel::mouseUp(const juce::MouseEvent& e) {
    const auto p = e.position;
    if (closeArea().contains(p)) { if (close) close(); return; }
    if (syncRow().contains(p)) proc.syncToHost = !proc.syncToHost.load();
    else if (midiRow().contains(p)) proc.midiOut = !proc.midiOut.load();
    else if (dragRow().contains(p)) proc.dragAllLayers = !proc.dragAllLayers.load();
    else if (licenseRow().contains(p)) { if (changeLicense) changeLicense(); return; }
    else if (updateRow().contains(p)) {
        if (update.state == UpdateStatus::available && update.url.isNotEmpty()) juce::URL(update.url).launchInDefaultBrowser();
        else if (update.state != UpdateStatus::checking && checkUpdate) checkUpdate();
    }
    else {
        for (size_t i = 0; i < bpmValues.size(); ++i) if (chip(i).contains(p)) proc.bpm = bpmValues[i];
        for (size_t i = 0; i < scaleValues.size(); ++i)
            if (scaleChip(i).contains(p) && setWindowScale) { setWindowScale(scaleValues[i]); return; }
    }
    if (changed) changed();
    repaint();
}

// ---------------------------------------------------------------------------

LicensePanel::LicensePanel(Mode m, juce::String currentEmail) : mode(m), email(std::move(currentEmail)) {
    using namespace Theme;
    field.setFont(font(15.0f));
    field.setTextToShowWhenEmpty("seuemail@gmail.com", quiet.brighter(0.3f));
    field.setIndents(40, 12);
    field.setJustification(juce::Justification::centredLeft);
    field.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);   // the rounded box is painted by the panel
    field.setColour(juce::TextEditor::textColourId, fg);
    field.setColour(juce::TextEditor::highlightColourId, acc.withAlpha(0.35f));
    field.setColour(juce::TextEditor::highlightedTextColourId, fg);
    field.setColour(juce::CaretComponent::caretColourId, acc);
    field.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    field.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    field.setInputRestrictions(254);
    if (mode == Mode::revoked) field.setText(email, false);
    field.onReturnKey = [this] { submit(); };
    field.onTextChange = [this] { if (failed) { failed = false; message.clear(); } repaint(); };
    addAndMakeVisible(field);
    addAndMakeVisible(activate);
    addChildComponent(cancel);
    cancel.setVisible(mode == Mode::change);
    closeBtn.ghost = true;
    addChildComponent(closeBtn);
    closeBtn.setVisible(mode == Mode::change);
    closeBtn.onClick = [this] { if (onCancel) onCancel(); };
    for (auto* b : { &activate, &cancel }) b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
    activate.onClick = [this] { submit(); };
    cancel.onClick = [this] { if (onCancel) onCancel(); };
    setSize(400, 316);
}

void LicensePanel::visibilityChanged() {
    if (isShowing()) juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::TextEditor>(&field)] {
        if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
    });
}

void LicensePanel::submit() {
    if (busy) return;
    busy = true;
    failed = false;
    message = juce::String::fromUTF8("Verificando sua compra...");
    activate.setEnabled(false);
    activate.setButtonText("VERIFICANDO...");
    field.setReadOnly(true);
    repaint();
    if (onSubmit) onSubmit(field.getText());
}

void LicensePanel::finish(bool ok, const juce::String& text) {
    busy = false;
    failed = !ok;
    message = text;
    activate.setEnabled(true);
    activate.setButtonText("ATIVAR");
    field.setReadOnly(false);
    repaint();
}

// Audio Makers UI dialog: 24 px padding, key badge, one field, the two buttons the same size with ATIVAR on the right.
void LicensePanel::resized() {
    field.setBounds(24, 164, getWidth() - 48, AMUI::Size::field);
    closeBtn.setBounds(getWidth() - 44, 16, 28, 28);
    auto row = juce::Rectangle<int>(24, getHeight() - 24 - AMUI::Size::button, getWidth() - 48, AMUI::Size::button);
    activate.setBounds(row.removeFromRight(cancel.isVisible() ? 128 : 160));
    if (cancel.isVisible()) { row.removeFromRight(10); cancel.setBounds(row.removeFromRight(128)); }
}

void LicensePanel::paint(juce::Graphics& g) {
    using namespace Theme;
    auto all = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(all, AMUI::Size::radiusWindow);
    g.setGradientFill(juce::ColourGradient(acc.withAlpha(0.10f), 0, 0, acc.withAlpha(0.0f), 0, 96, false));
    g.fillRoundedRectangle(all, AMUI::Size::radiusWindow);
    g.setColour(line);
    g.drawRoundedRectangle(all.reduced(0.5f), AMUI::Size::radiusWindow, 1);

    auto badge = juce::Rectangle<float>(24, 24, 40, 40);
    g.setColour(acc.withAlpha(0.12f)); g.fillRoundedRectangle(badge, 10);
    g.setColour(acc.withAlpha(0.35f)); g.drawRoundedRectangle(badge.reduced(0.5f), 10, 1);
    drawIcon(g, Icon::key, juce::Rectangle<float>(20, 20).withCentre(badge.getCentre()), acc);
    g.setColour(mode == Mode::revoked ? danger : mute); g.setFont(label());
    g.drawText(juce::String::fromUTF8(mode == Mode::revoked ? "LICENÇA DESATIVADA" : "LICENÇA"), 78, 25, 240, 16, juce::Justification::centredLeft);
    g.setColour(fg); g.setFont(font(18.0f, true));
    g.drawText(juce::String::fromUTF8(mode == Mode::activate ? "Ative o Melody Code" : mode == Mode::revoked ? "Seu acesso foi desativado"
                                                                                                        : "Trocar e-mail da licença"),
               78, 41, getWidth() - 126, 24, juce::Justification::centredLeft);

    juce::String text;
    if (mode == Mode::activate)
        text = juce::String::fromUTF8("Digite o e-mail usado na compra do Melody Code.");
    else if (mode == Mode::revoked)
        text = juce::String::fromUTF8("O acesso de ") + email + juce::String::fromUTF8(" não está mais ativo. Informe um e-mail com acesso ativo.");
    else
        text = juce::String::fromUTF8("Informe outro e-mail com acesso ativo ao Melody Code.");
    g.setColour(mute); g.setFont(font(14.0f));
    g.drawFittedText(text, 24, 82, getWidth() - 48, 42, juce::Justification::topLeft, 2, 1.0f);

    g.setColour(mute); g.setFont(label());
    g.drawText(juce::String::fromUTF8("E-MAIL DA COMPRA"), 24, 142, 240, 16, juce::Justification::centredLeft);
    {   // the field's box: focused from the moment the panel opens
        const auto box = field.getBounds().toFloat();
        g.setColour(acc.withAlpha(0.12f)); g.drawRoundedRectangle(box.expanded(2.0f), AMUI::Size::radiusControl + 2, 3.0f);
        g.setColour(bg); g.fillRoundedRectangle(box, AMUI::Size::radiusControl);
        g.setColour(acc.withAlpha(0.8f)); g.drawRoundedRectangle(box.reduced(0.5f), AMUI::Size::radiusControl, 1.0f);
        drawIcon(g, Icon::mail, { box.getX() + 14, box.getCentreY() - 8, 16.0f, 16.0f }, acc);
    }
    g.setColour(message.isNotEmpty() ? (failed ? danger : mute) : quiet.brighter(0.2f));
    g.setFont(font(12.5f));
    g.drawFittedText(message.isNotEmpty() ? message : juce::String::fromUTF8("Verificação só na primeira vez; depois funciona offline."),
                     24, 214, getWidth() - 48, 30, juce::Justification::topLeft, 2, 1.0f);
}

void DragMidiButton::mouseDrag(const juce::MouseEvent& e) {
    if (e.getDistanceFromDragStart() < 6 || !files) return;
    if (auto list = files(); !list.isEmpty())
        if (!juce::DragAndDropContainer::performExternalDragDropOfFiles(list, false, this) && dragFailed) dragFailed();
}

void LockButton::paintButton(juce::Graphics& g, bool over, bool) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(locked ? Theme::acc : juce::Colour(0xaa000000));
    g.fillRoundedRectangle(r, 7);
    g.setColour(locked ? Theme::acc : (over ? Theme::acc.withAlpha(0.6f) : Theme::line));
    g.drawRoundedRectangle(r, 7, 1);
    const auto c = locked ? Theme::ink : Theme::mute;
    auto body = juce::Rectangle<float>(0, 0, 12, 9).withCentre(r.getCentre().translated(0, 2.5f));
    g.setColour(c);
    g.fillRoundedRectangle(body, 2);
    juce::Path shackle;
    const float sx = body.getX() + 2.5f, ex = body.getRight() - 2.5f, top = body.getY() - 6;
    shackle.startNewSubPath(sx, body.getY());
    shackle.lineTo(sx, top + 3);
    shackle.quadraticTo(sx, top, (sx + ex) * 0.5f, top);
    shackle.quadraticTo(ex, top, ex, top + 3);
    if (locked) shackle.lineTo(ex, body.getY());
    g.strokePath(shackle, juce::PathStrokeType(1.8f));
}

// ---------------------------------------------------------------- piano roll
PianoRoll::PianoRoll(MelodyCodeProcessor& p) : proc(p) {
    setWantsKeyboardFocus(true);
    refresh();
}

juce::Rectangle<float> PianoRoll::noteArea() const {
    return getLocalBounds().toFloat().withTrimmedTop((float) headerH).withTrimmedBottom((float) subH).withTrimmedLeft((float) keyW);
}
float PianoRoll::stepW() const { return noteArea().getWidth() / (float) (pat.bars() * mc::stepsPerBar); }
float PianoRoll::rowH() const { return noteArea().getHeight() / (float) (hi - lo + 1); }
double PianoRoll::stepAt(float x) const { return (x - noteArea().getX()) / stepW(); }
int PianoRoll::pitchAt(float y) const { return hi - (int) std::floor((y - noteArea().getY()) / rowH()); }

juce::Rectangle<float> PianoRoll::noteRect(const mc::Note& n) const {
    const auto a = noteArea();
    const float x = a.getX() + (float) n.start / mc::step * stepW();
    const float w = juce::jmax(5.0f, (float) n.length / mc::step * stepW());
    return { x + 1, a.getY() + (float) (hi - n.pitch) * rowH() + 1, w - 2, rowH() - 2 };
}

PianoRoll::Hit PianoRoll::hit(juce::Point<float> p) const {
    for (int i = (int) notes.size() - 1; i >= 0; --i) {
        auto r = noteRect(notes[(size_t) i]).expanded(1, 0);
        if (r.contains(p)) return { i, p.x > r.getRight() - juce::jmin(8.0f, r.getWidth() * 0.35f) };
    }
    return {};
}

void PianoRoll::refresh() {
    pat = proc.pattern();
    if (drag.index < 0) notes = pat.melody;
    if (selected >= (int) notes.size()) selected = -1;
    if (!notes.empty() && drag.index < 0) {
        int mn = 127, mx = 0;
        for (auto& n : notes) { mn = std::min(mn, n.pitch); mx = std::max(mx, n.pitch); }
        lo = mn - 3; hi = mx + 3;
        if (hi - lo < 20) { const int extra = 20 - (hi - lo); lo -= extra / 2; hi += extra - extra / 2; }
    }
    repaint();
}

void PianoRoll::resized() {}

void PianoRoll::paint(juce::Graphics& g) {
    auto all = getLocalBounds().toFloat();
    juce::Path clip; clip.addRoundedRectangle(all, 14);
    g.reduceClipRegion(clip);
    g.fillAll(Theme::panel);
    const auto a = noteArea();
    const float rh = rowH(), sw = stepW();
    const auto scale = ccp::HarmonyEngine::scaleNotes(pat.key, pat.scale);
    auto inScale = [&](int m) { return std::find(scale.begin(), scale.end(), m % 12) != scale.end(); };
    const int bars = pat.bars(), steps = bars * mc::stepsPerBar;

    // rows
    for (int m = hi; m >= lo; --m) {
        const float y = a.getY() + (float) (hi - m) * rh;
        g.setColour(inScale(m) ? juce::Colour(0xff161a22) : juce::Colour(0xff0e1015));
        g.fillRect(a.getX(), y, a.getWidth(), rh);
    }
    // keyboard: a real piano on its side, white keys full width with black keys on top (Ed, 22/11)
    {
        const float kw = (float) keyW - 1.0f, bw = std::round(kw * 0.62f);
        g.setColour(juce::Colour(0xffe6e9ef));
        g.fillRect(0.0f, a.getY(), kw, a.getHeight());
        for (int m = hi; m >= lo; --m) {
            const float y = a.getY() + (float) (hi - m) * rh;
            const int pc = m % 12;
            g.setColour(juce::Colour(0xffb4bac6));
            if (pc == 0 || pc == 5) g.fillRect(0.0f, y + rh - 0.5f, kw, 1.0f);           // C|B and F|E meet without a black key
            if (isBlack(m)) g.fillRect(bw, y + rh * 0.5f - 0.5f, kw - bw, 1.0f);          // white keys split behind the black one
            if (pc == 0) {
                g.setColour(juce::Colour(0xff6b7280));
                g.setFont(Theme::semi(9.5f));
                g.drawText("C" + juce::String(m / 12 - 1), juce::Rectangle<float>(0, y, kw - 4, rh), juce::Justification::centredRight);
            }
        }
        for (int m = hi; m >= lo; --m) {
            if (!isBlack(m)) continue;
            const float y = a.getY() + (float) (hi - m) * rh;
            auto key = juce::Rectangle<float>(0.0f, y + 0.5f, bw, rh - 1.0f);
            g.setColour(juce::Colour(0xff111318));
            g.fillRoundedRectangle(key.withTrimmedLeft(-3.0f), 2.0f);
            g.setColour(juce::Colour(0xff2c313b));
            g.fillRect(key.getX(), key.getY() + 1.0f, key.getWidth() - 3.0f, 1.0f);
        }
        g.setColour(Theme::line);
        g.fillRect(kw, a.getY(), 1.0f, a.getHeight());
    }
    // chord tones per bar
    for (int st = 0; st < bars * 16; ++st) {   // chord tones follow where each chord really sounds
        auto pcs = mc::Engine::chordPitchClasses(pat.chordAt(st * mc::step));
        for (int m = hi; m >= lo; --m)
            if (std::find(pcs.begin(), pcs.end(), m % 12) != pcs.end()) {
                g.setColour(Theme::acc.withAlpha(0.06f));
                g.fillRect(a.getX() + (float) st * sw, a.getY() + (float) (hi - m) * rh, sw, rh);
            }
    }
    for (int b = 0; b < bars; ++b) {
        if (b > 0 && pat.chordShift[(size_t) b] != 0) {   // chord change off the bar line
            const float x = a.getX() + (float) (b * 16 + pat.chordShift[(size_t) b]) * sw;
            g.setColour(Theme::acc2.withAlpha(0.8f));
            g.fillRect(x - 1.0f, a.getY(), 2.0f, a.getHeight());
        }
    }
    // header + sub lane backgrounds
    g.setColour(Theme::panel);
    g.fillRect(0.0f, 0.0f, all.getWidth(), (float) headerH);
    const auto lane = juce::Rectangle<float>(a.getX(), a.getBottom(), a.getWidth(), (float) subH);
    g.setColour(juce::Colour(0xff0e1015));
    g.fillRect(0.0f, lane.getY(), all.getWidth(), lane.getHeight());
    g.setColour(Theme::line);
    g.fillRect(0.0f, lane.getY(), all.getWidth(), 1.0f);
    g.setColour(Theme::mute);
    g.setFont(Theme::label(10.0f));
    g.drawText("GRAVE", juce::Rectangle<float>(0, lane.getY(), (float) keyW, lane.getHeight()), juce::Justification::centred);
    // chord names where each chord really starts (generated for the whole loop, not picked per bar)
    for (int b = 0; b < bars; ++b) {
        if (b > 0 && pat.progression.chords[(size_t) b].name == pat.progression.chords[(size_t) (b - 1)].name && pat.chordShift[(size_t) b] == 0) continue;
        const auto& c = pat.progression.chords[(size_t) b];
        const float x = a.getX() + (float) (b * 16 + pat.chordShift[(size_t) b]) * sw;
        const auto deg = juce::String(roman[c.degree % 7]);
        auto tag = juce::Rectangle<float>(x + 8, 9, juce::GlyphArrangement::getStringWidth(Theme::font(11.0f, true), deg) + 12, 20);
        g.setColour(Theme::acc.withAlpha(0.12f));
        g.fillRoundedRectangle(tag, AMUI::Size::radiusTag);
        g.setColour(Theme::acc);
        g.setFont(Theme::font(11.0f, true));
        g.drawText(deg, tag, juce::Justification::centred);
        g.setColour(Theme::fg);
        g.setFont(Theme::semi(13.0f));
        g.drawText(c.name, juce::Rectangle<float>(tag.getRight() + 8, 7, 110, 24), juce::Justification::centredLeft);
    }
    // grid
    for (int s = 0; s <= steps; ++s) {
        g.setColour(s % 16 == 0 ? Theme::line2 : s % 4 == 0 ? juce::Colour(0xff21252e) : juce::Colour(0xff181b22));
        g.fillRect(a.getX() + (float) s * sw, s % 16 == 0 ? 0.0f : a.getY(), 1.0f, all.getHeight());
    }
    // sub (ghost)
    int subLo = 127, subHi = 0;
    for (auto& n : pat.bass) { subLo = std::min(subLo, n.pitch); subHi = std::max(subHi, n.pitch); }
    const float laneH = lane.getHeight() - 14;
    for (auto& n : pat.bass) {
        const float x = a.getX() + (float) n.start / mc::step * sw;
        const float w = juce::jmax(4.0f, (float) n.length / mc::step * sw - 2);
        const float rel = subHi > subLo ? (float) (n.pitch - subLo) / (float) (subHi - subLo) : 0.5f;
        auto r = juce::Rectangle<float>(x + 1, lane.getY() + 7 + (1.0f - rel) * (laneH - 12), w, 12);
        g.setColour(Theme::mute.withAlpha(0.18f));
        g.fillRoundedRectangle(r, 4);
        juce::Path outline; outline.addRoundedRectangle(r, 4);
        juce::Path dashed; const float dashes[] { 4, 3 };
        juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
        g.setColour(Theme::mute.withAlpha(0.5f));
        g.fillPath(dashed);
        if (n.start % (mc::stepsPerBar * mc::step) == 0 && w > 26) {
            g.setColour(Theme::mute);
            g.setFont(Theme::font(10.0f, true));
            g.drawText(juce::String(noteNames[n.pitch % 12]) + juce::String(n.pitch / 12 - 1), r.withTrimmedLeft(4), juce::Justification::centredLeft);
        }
    }
    // melody
    for (int i = 0; i < (int) notes.size(); ++i) {
        const auto& n = notes[(size_t) i];
        auto r = noteRect(n);
        juce::Path p; p.addRoundedRectangle(r, 4);
        juce::DropShadow(Theme::acc.withAlpha(i == selected ? 0.7f : 0.35f), i == selected ? 14 : 9, {}).drawForPath(g, p);
        g.setGradientFill(juce::ColourGradient(Theme::acc, r.getX(), 0, Theme::acc2, r.getRight() + 30, 0, false));
        g.setOpacity(0.55f + 0.45f * (float) n.velocity / 127.0f);
        g.fillPath(p);
        g.setOpacity(1.0f);
        if (i == selected) { g.setColour(juce::Colours::white); g.strokePath(p, juce::PathStrokeType(2.0f)); }
        g.setColour(juce::Colours::white.withAlpha(0.35f));
        g.fillRect(r.getRight() - 3, r.getY() + 3, 1.5f, r.getHeight() - 6);
    }
    // playhead
    if (const double t = proc.playheadTicks(); t >= 0) {
        g.setColour(juce::Colours::white);
        g.fillRect(a.getX() + (float) (t / mc::step) * sw, 0.0f, 2.0f, all.getHeight());
    }
    g.setColour(Theme::line);
    g.fillRect(0.0f, (float) headerH - 1.0f, all.getWidth(), 1.0f);
    g.drawRoundedRectangle(all.reduced(0.5f), AMUI::Size::radiusPanel, 1);
}

void PianoRoll::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto p = e.position;
    if (!noteArea().contains(p)) return;
    auto h = hit(p);
    if (e.mods.isPopupMenu()) {
        if (h.index >= 0) { notes.erase(notes.begin() + h.index); selected = -1; commit(); }
        return;
    }
    if (h.index < 0) {
        const int st = (int) std::floor(stepAt(p.x));
        const int bar = st / mc::stepsPerBar;
        if (bar < 0 || bar >= pat.bars()) return;
        notes.push_back({ st * mc::step, 2 * mc::step, pitchAt(p.y), 100 });
        h = { (int) notes.size() - 1, false };
    }
    auto& n = notes[(size_t) h.index];
    selected = h.index;
    drag = { h.index, h.edge, stepAt(p.x) - (double) n.start / mc::step, pitchAt(p.y), n.pitch, false };
    proc.audition(n.pitch);
    repaint();
}

void PianoRoll::mouseDrag(const juce::MouseEvent& e) {
    if (drag.index < 0 || drag.index >= (int) notes.size()) return;
    auto& n = notes[(size_t) drag.index];
    const int total = pat.bars() * mc::stepsPerBar;
    if (drag.resize) {
        const int startStep = n.start / mc::step;
        n.length = juce::jlimit(1, total - startStep, (int) std::round(stepAt(e.position.x) - startStep)) * mc::step;
    } else {
        const int s = juce::jlimit(0, total - 1, (int) std::round(stepAt(e.position.x) - drag.offset));
        n.start = s * mc::step;
        const int np = juce::jlimit(lo, hi, drag.startPitch + (pitchAt(e.position.y) - drag.pitch0));
        if (np != n.pitch) { n.pitch = np; proc.audition(np); }
    }
    drag.moved = true;
    repaint();
}

void PianoRoll::mouseUp(const juce::MouseEvent&) {
    if (drag.index >= 0) {
        // keep the selection on the same note after sorting
        const auto keep = notes[(size_t) drag.index];
        drag.index = -1;
        commit();
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[(size_t) i].start == keep.start && notes[(size_t) i].pitch == keep.pitch) selected = i;
        repaint();
    }
}

void PianoRoll::mouseMove(const juce::MouseEvent& e) {
    if (!noteArea().contains(e.position)) { setMouseCursor(juce::MouseCursor::NormalCursor); return; }
    const auto h = hit(e.position);
    setMouseCursor(h.index < 0 ? juce::MouseCursor::CrosshairCursor
                   : h.edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
}

void PianoRoll::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto h = hit(e.position);
    if (h.index >= 0) { notes.erase(notes.begin() + h.index); selected = -1; commit(); }
}

bool PianoRoll::keyPressed(const juce::KeyPress& k) {
    if (selected < 0 || selected >= (int) notes.size()) return false;
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) {
        notes.erase(notes.begin() + selected); selected = -1; commit(); return true;
    }
    if (k.getKeyCode() == juce::KeyPress::upKey || k.getKeyCode() == juce::KeyPress::downKey) {
        const int d = (k.getKeyCode() == juce::KeyPress::upKey ? 1 : -1) * (k.getModifiers().isShiftDown() ? 12 : 1);
        auto& n = notes[(size_t) selected];
        n.pitch = juce::jlimit(24, 108, n.pitch + d);
        proc.audition(n.pitch);
        const auto keep = n;
        commit();
        for (int i = 0; i < (int) notes.size(); ++i)
            if (notes[(size_t) i].start == keep.start && notes[(size_t) i].pitch == keep.pitch) selected = i;
        return true;
    }
    return false;
}

void PianoRoll::commit() {
    proc.setMelody(notes);   // triggers refresh() through the change broadcaster
}

// ---------------------------------------------------------------- editor
MelodyCodeEditor::MelodyCodeEditor(MelodyCodeProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&look);
    addAndMakeVisible(content);

    keyBox.addItem(utf8("Padr\xc3\xa3o"), 1);
    for (int i = 0; i < 12; ++i) keyBox.addItem(noteNames[i], i + 2);
    scaleBox.addItem(utf8("Padr\xc3\xa3o"), 1);
    for (int i = 0; i < (int) ccp::ScaleType::count; ++i) scaleBox.addItem(AMUI::displayCase(utf8(ccp::scaleName((ccp::ScaleType) i))), i + 2);
    for (int i = 0; i < (int) mc::Style::count; ++i) styleBox.addItem(AMUI::displayCase(utf8(mc::styleName((mc::Style) i))), i + 1);
    for (int i = 0; i < (int) mc::Vibe::count; ++i) vibeBox.addItem(AMUI::displayCase(utf8(mc::vibeName((mc::Vibe) i))), i + 1);
    for (int k = 0; k < Sets::kitCount; ++k) soundBox.addItem(AMUI::displayCase(utf8(Sets::kit(k).name)), k + 1);
    auto onSetting = [this] {
        auto& s = proc.settings();
        s.key = keyBox.getSelectedId() - 2;      // id 1 = PADRÃO (-1)
        s.scale = scaleBox.getSelectedId() - 2;
        s.style = (mc::Style) (styleBox.getSelectedId() - 1);
        refreshAfterSetting(true);
    };
    for (auto* b : { &keyBox, &scaleBox, &styleBox }) { b->onChange = onSetting; content.addAndMakeVisible(b); }
    // the vibe changes the preview sound now and the phrase character from the next generation (notes on screen stay)
    vibeBox.onChange = [this] { proc.setVibe((mc::Vibe) (vibeBox.getSelectedId() - 1)); };
    content.addAndMakeVisible(vibeBox);
    // changing the sound only changes the preview instrument: the notes, history and MIDI stay as they are
    soundBox.onChange = [this] {
        proc.kit = soundBox.getSelectedId() - 1;
        soundBox.setTooltip(utf8(Sets::kit(proc.kit).description));
        if (const int v = Sets::kit(proc.kit).vibe; v >= 0) vibeBox.setSelectedId(v + 1);   // sets that sound best with an effect bring it along
    };
    content.addAndMakeVisible(soundBox);
    auto step = [this](int d) { soundBox.setSelectedId((soundBox.getSelectedId() - 1 + d + Sets::kitCount) % Sets::kitCount + 1); };
    soundPrev.onClick = [step] { step(-1); }; soundNext.onClick = [step] { step(1); };
    for (auto* b : { &soundPrev, &soundNext }) { b->ghost = true; b->setTooltip(utf8("Timbre anterior / pr\xc3\xb3ximo")); content.addAndMakeVisible(b); }
    content.addAndMakeVisible(roll);

    role.onChange = [this](int i) { proc.settings().role = (mc::Role) i; refreshAfterSetting(false); };
    density.onChange = [this](int i) { proc.settings().density = (mc::Density) i; refreshAfterSetting(false); };
    range.onChange = [this](int i) { proc.settings().range = (mc::Range) i; refreshAfterSetting(false); };
    feel.onChange = [this](int i) { proc.settings().feel = (mc::Feel) i; refreshAfterSetting(false); };
    bars.onChange = [this](int i) { proc.settings().bars = i == 0 ? 2 : i == 1 ? 4 : 8; refreshAfterSetting(true); };
    for (auto* s : { &role, &feel, &density, &range, &bars }) content.addAndMakeVisible(s);

    auto setupSlider = [this](juce::Slider& s, double lo, double hi, double v, std::function<void(double)> f) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s.setRange(lo, hi, 1);
        s.setValue(v, juce::dontSendNotification);
        s.onValueChange = [&s, f] { f(s.getValue()); };
        content.addAndMakeVisible(s);
    };
    setupSlider(bpm, 70, 170, proc.bpm, [this](double v) { proc.bpm = (int) v; content.repaint(); });
    setupSlider(bassVol, 0, 100, proc.bassLevel * 100, [this](double v) { proc.bassLevel = (float) v / 100.0f; content.repaint(); });
    setupSlider(melVol, 0, 100, proc.melodyLevel * 100, [this](double v) { proc.melodyLevel = (float) v / 100.0f; content.repaint(); });
    setupSlider(chordVol, 0, 100, proc.chordLevel * 100, [this](double v) { proc.chordLevel = (float) v / 100.0f; content.repaint(); });

    // hover explanations for the controls under the piano roll
    role.setTooltip(utf8("Fun\xc3\xa7\xc3\xa3o da melodia. L\xc3\x8d" "DER: a linha que canta a m\xc3\xbasica. REFR\xc3\x83O: um gancho curto e repetido, f\xc3\xa1" "cil de lembrar. RESPOSTA: uma linha que responde \xc3\xa0 principal, com mais espa\xc3\xa7o."));
    feel.setTooltip(utf8("Levada do ritmo. RETO: notas no tempo. SWING: as notas do contratempo atrasam um pouco, balan\xc3\xa7" "ado. TERCINA: grupos de tr\xc3\xaas notas, como em trap e R&B."));
    density.setTooltip(utf8("Quantas notas a melodia tem. BAIXA deixa mais espa\xc3\xa7o e pausas, ALTA \xc3\xa9 mais movimentada."));
    range.setTooltip(utf8("Altura da melodia. GRAVE fica mais embaixo, AGUDO mais em cima."));
    bars.setTooltip(utf8("Tamanho do loop em compassos: 2, 4 ou 8."));
    bpm.setTooltip(utf8("Andamento do preview e do WAV. Com a DAW tocando e a sincronia ligada, o plugin segue o tempo da DAW."));
    bassVol.setTooltip(utf8("Volume do grave no preview e no WAV. N\xc3\xa3o muda as notas."));
    melVol.setTooltip(utf8("Volume da melodia no preview e no WAV. N\xc3\xa3o muda as notas."));
    chordVol.setTooltip(utf8("Volume dos acordes no preview e no WAV. N\xc3\xa3o muda as notas."));
    historyBtn.setTooltip(utf8("Volta para uma gera\xc3\xa7\xc3\xa3o anterior."));

    play.onClick = [this] { proc.setPlaying(!proc.isPlaying()); };
    vari.onClick = [this] { proc.variation(); };
    newMel.onClick = [this] { proc.newMelody(); };
    gen.onClick = [this] { proc.generateAll(); };
    save.onClick = [this] { saveMidi(); };
    // A click (no drag) writes the files and opens their folder, so they can be dragged from there.
    dragBtn.onClick = [this] { revealForDrag(false); };
    dragBtn.dragFailed = [this] { revealForDrag(false); };
    dragBtn.files = [this] {
        juce::String error;
        auto list = dragMidiFiles(error);
        if (list.isEmpty()) showProblem(utf8("N\xc3\xa3o deu pra criar o MIDI"), error.trim());
        return list;
    };
    dragWav.onClick = [this] { revealForDrag(true); };
    dragWav.dragFailed = [this] { revealForDrag(true); };
    dragWav.files = [this] {
        juce::String error;
        const auto f = dragWavFile(error);
        if (!f.existsAsFile()) { showProblem(utf8("N\xc3\xa3o deu pra criar o WAV"), error.trim()); return juce::StringArray(); }
        return juce::StringArray(f.getFullPathName());
    };
    historyBtn.onClick = [this] {
        juce::PopupMenu m;
        for (int i = 0; i < proc.historySize(); ++i) m.addItem(i + 1, proc.historyLabel(i));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(historyBtn), [this](int r) { if (r > 0) proc.restoreHistory(r - 1); });
    };
    for (auto* b : std::initializer_list<juce::Button*> { &play, &vari, &newMel, &gen, &historyBtn, &save, &dragBtn, &dragWav }) {
        b->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        content.addAndMakeVisible(b);
    }

    gear.onClick = [this] { openSettings(); };
    checkForUpdates(true);   // silent: only lights the dot on the gear
    content.addAndMakeVisible(gear);
    addChildComponent(overlay);

    proc.addChangeListener(this);
    syncControls();
    constrainer.setFixedAspectRatio((double) Theme::width / Theme::height);
    constrainer.setSizeLimits(Theme::width / 2, Theme::height / 2, Theme::width * 2, Theme::height * 2);
    setConstrainer(&constrainer);
    setResizable(true, true);
    const float scale = license->getWindowScale();
    setSize(juce::roundToInt(Theme::width * scale), juce::roundToInt(Theme::height * scale));
    license->addChangeListener(this);
    startTimerHz(30);
    juce::MessageManager::callAsync([safe = juce::Component::SafePointer<MelodyCodeEditor>(this)] {
        if (safe != nullptr) safe->checkLicense();
    });
}

MelodyCodeEditor::~MelodyCodeEditor() {
    updateChecker.cancelAndWait();
    license->removeChangeListener(this);
    proc.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void MelodyCodeEditor::syncControls() {
    const auto& s = proc.settings();
    keyBox.setSelectedId(s.key + 2, juce::dontSendNotification);
    scaleBox.setSelectedId(s.scale + 2, juce::dontSendNotification);
    styleBox.setSelectedId((int) s.style + 1, juce::dontSendNotification);
    vibeBox.setSelectedId((int) s.vibe + 1, juce::dontSendNotification);
    soundBox.setSelectedId(proc.kit + 1, juce::dontSendNotification);
    role.setSelected((int) s.role);
    density.setSelected((int) s.density);
    range.setSelected((int) s.range);
    feel.setSelected((int) s.feel);
    bars.setSelected(s.bars == 2 ? 0 : s.bars == 4 ? 1 : 2);
    bpm.setValue(proc.bpm, juce::dontSendNotification);
    play.setButtonText(proc.isPlaying() ? "PARAR" : "OUVIR");
    play.icon = proc.isPlaying() ? Icon::stop : Icon::play;
    play.lit = proc.isPlaying();
    play.repaint();
    roll.refresh();
    content.repaint();
}

void MelodyCodeEditor::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == static_cast<juce::ChangeBroadcaster*>(&license.getObject())) {
        // A revoked license locks the window again; a new activation is handled by its panel.
        if (license->getStatus() != LicenseManager::Status::active) checkLicense();
        overlay.repaint();
        return;
    }
    syncControls();
}

void MelodyCodeEditor::timerCallback() {
    if (proc.isPlaying()) roll.repaint();
    if (++licenseTicks >= 30 * 60) { licenseTicks = 0; license->revalidate(); }
    if (hintUntil && juce::Time::getMillisecondCounter() > hintUntil) { hintUntil = 0; hint.clear(); content.repaint(); }
}

// A changed setting rewrites the pattern right away, unless the user has edited the notes by hand:
// then the edit is kept and the setting applies to the next GERAR / NOVA MELODIA / VARIAÇÃO.
void MelodyCodeEditor::refreshAfterSetting(bool harmonyToo) {
    if (proc.melodyEdited()) { showHint(utf8("Notas editadas mantidas: a mudan\xc3\xa7" "a vale na pr\xc3\xb3xima gera\xc3\xa7\xc3\xa3o")); return; }
    if (harmonyToo) proc.applySettings(); else proc.newMelody();
}

void MelodyCodeEditor::showHint(const juce::String& text) {
    hint = text;
    hintUntil = juce::Time::getMillisecondCounter() + 2500;
    content.repaint();
}

juce::StringArray MelodyCodeEditor::writeMidiFiles(const juce::File& folder, const juce::String& base, juce::String& error) {
    const auto p = proc.pattern();
    juce::StringArray out;
    auto write = [&](const std::vector<mc::Note>& notes, const juce::String& suffix, int channel) {
        auto bytes = mc::midiBytes(notes, proc.bpm, channel, p.lengthTicks());
        auto f = folder.getChildFile(base + " - " + suffix + ".mid");
        juce::String why;
        if (writeBytes(f, bytes.data(), bytes.size(), why)) out.add(f.getFullPathName());
        else error << f.getFullPathName() << "  ->  " << why << "\n";
    };
    write(p.melody, "Melody", 0);
    write(p.bass, "Bass", 1);
    write(p.chords, "Chords", 2);
    return out;
}

juce::StringArray MelodyCodeEditor::dragMidiFiles(juce::String& error) {
    for (const auto& dir : dragFolders()) {
        auto list = writeMidiFiles(dir, "Melody Code", error);
        if (list.size() == 3) {
            if (!proc.dragAllLayers.load()) list.removeRange(1, list.size() - 1);   // melody only
            return list;
        }
    }
    return {};
}

juce::File MelodyCodeEditor::dragWavFile(juce::String& error) {
    const bool all = proc.dragAllLayers.load();
    for (const auto& dir : dragFolders()) {
        const auto f = dir.getChildFile(all ? "Melody Code - Full.wav" : "Melody Code - Melody.wav");
        if (const auto made = dir.createDirectory(); made.failed()) { error << dir.getFullPathName() << "  ->  " << made.getErrorMessage() << "\n"; continue; }
        if (proc.renderWav(f, all) && f.existsAsFile() && f.getSize() > 44) return f;
        error << f.getFullPathName() << "  ->  " << utf8("n\xc3\xa3o foi poss\xc3\xadvel gravar") << "\n";
    }
    return {};
}

void MelodyCodeEditor::revealForDrag(bool wav) {
    juce::String error;
    juce::File file;
    if (wav) file = dragWavFile(error);
    else if (const auto list = dragMidiFiles(error); !list.isEmpty()) file = juce::File(list[0]);
    if (!file.existsAsFile()) {
        showProblem(wav ? utf8("N\xc3\xa3o deu pra criar o WAV") : utf8("N\xc3\xa3o deu pra criar o MIDI"),
                    utf8("O Windows bloqueou a grava\xc3\xa7\xc3\xa3o em todas as pastas que o Melody Code tentou.\n\n") + error.trim()
                    + utf8("\n\nSe o \"Acesso controlado a pastas\" do Windows Defender estiver ligado, libere a sua DAW e o Melody Code nele."));
        return;
    }
    file.revealToUser();
    showProblem(wav ? utf8("WAV pronto") : utf8("MIDI pronto"),
                utf8("Abrimos a pasta com o arquivo:\n") + file.getFileName()
                + utf8("\n\nArraste ele da pasta direto para a sua DAW.\n(Dica: para arrastar do plugin, clique no bot\xc3\xa3o e, sem soltar, puxe at\xc3\xa9 a DAW.)"));
}

void MelodyCodeEditor::showProblem(const juce::String& title, const juce::String& message) {
    juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::NoIcon)
                                     .withTitle(title).withMessage(message).withButton("OK").withAssociatedComponent(this), nullptr);
}

void MelodyCodeEditor::saveMidi() {
    chooser = std::make_unique<juce::FileChooser>("Salvar MIDI", juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("Melody Code.mid"), "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            juce::String why;
            const auto files = writeMidiFiles(f.getParentDirectory(), f.getFileNameWithoutExtension(), why);
            if (files.size() == 3) { showHint("Salvo: " + f.getFileNameWithoutExtension() + " - Melody, Bass e Chords.mid"); return; }
            // the chosen folder refused the write (Controlled folder access, OneDrive offline...): save somewhere safe and show it
            juce::String error;
            for (const auto& dir : dragFolders())
                if (const auto saved = writeMidiFiles(dir, f.getFileNameWithoutExtension(), error); saved.size() == 3) {
                    juce::File(saved[0]).revealToUser();
                    showProblem(utf8("MIDI salvo em outra pasta"), utf8("O Windows n\xc3\xa3o deixou salvar em:\n") + f.getParentDirectory().getFullPathName()
                                + "\n(" + why.trim() + ")" + utf8("\n\nSalvamos aqui e abrimos a pasta:\n") + dir.getFullPathName());
                    return;
                }
            showProblem(utf8("N\xc3\xa3o deu pra salvar o MIDI"), why.trim() + "\n\n" + error.trim());
        });
}

void MelodyCodeEditor::openSettings() {
    auto panel = std::make_unique<SettingsPanel>(proc, update, [this] { overlay.dismiss(); }, [this] { bpm.setValue(proc.bpm, juce::dontSendNotification); content.repaint(); },
                                                 [this] { checkForUpdates(false); });
    juce::Component::SafePointer<MelodyCodeEditor> safe(this);
    panel->windowScale = [safe] { return safe != nullptr ? (float) safe->getWidth() / (float) Theme::width : 1.0f; };
    panel->setWindowScale = [safe](float s) { if (safe != nullptr) safe->applyWindowScale(s); };
    panel->licenseEmail = [safe] { return safe != nullptr ? safe->license->getEmail() : juce::String(); };
    panel->changeLicense = [safe] { if (safe != nullptr) safe->showLicense(LicensePanel::Mode::change); };
    overlay.present(*this, std::move(panel), (float) getWidth() / (float) Theme::width);
    overlay.grabKeyboardFocus();
}

// Resizes the open window and keeps the size as the default for every new Melody Code window.
void MelodyCodeEditor::applyWindowScale(float scale) {
    license->setWindowScale(scale);
    setSize(juce::roundToInt(Theme::width * scale), juce::roundToInt(Theme::height * scale));
    overlay.repaint();
}

void MelodyCodeEditor::checkLicense() {
    const auto status = license->getStatus();
    if (status == LicenseManager::Status::active) {
        if (dynamic_cast<LicensePanel*>(overlay.getPanel()) != nullptr) overlay.dismiss();
        license->revalidate(true);
        return;
    }
    if (auto* current = dynamic_cast<LicensePanel*>(overlay.getPanel()); current != nullptr && current->isLocked()) return;
    showLicense(status == LicenseManager::Status::revoked ? LicensePanel::Mode::revoked : LicensePanel::Mode::activate);
}

void MelodyCodeEditor::showLicense(LicensePanel::Mode mode) {
    auto panel = std::make_unique<LicensePanel>(mode, license->getEmail());
    juce::Component::SafePointer<MelodyCodeEditor> safe(this);
    juce::Component::SafePointer<LicensePanel> safePanel(panel.get());
    panel->onSubmit = [safe, safePanel](juce::String email) {
        if (safe == nullptr) return;
        safe->license->activate(email, [safe, safePanel](LicenseManager::Outcome outcome) {
            const bool ok = outcome.kind == LicenseManager::Outcome::Kind::authorized;
            if (safePanel != nullptr) safePanel->finish(ok, outcome.message);
            if (ok && safe != nullptr && safe->overlay.getPanel() == safePanel.getComponent()) safe->overlay.dismiss();
        });
    };
    panel->onCancel = [safe] { if (safe != nullptr) safe->openSettings(); };
    const bool locked = mode != LicensePanel::Mode::change;
    overlay.present(*this, std::move(panel), (float) getWidth() / (float) Theme::width, locked);
    overlay.grabKeyboardFocus();
}

void MelodyCodeEditor::checkForUpdates(bool silent) {
    if (updateChecker.isChecking()) return;
    if (!silent) update.state = UpdateStatus::checking;
    updateChecker.start(JucePlugin_VersionString, [safe = juce::Component::SafePointer<MelodyCodeEditor>(this), silent](UpdateChecker::Result r) {
        if (safe == nullptr) return;
        auto& u = safe->update;
        if (r.succeeded) { u.state = r.updateAvailable ? UpdateStatus::available : UpdateStatus::upToDate; u.latest = r.latestVersion; u.url = r.downloadUrl; }
        else if (!silent) u.state = UpdateStatus::failed;
        safe->gear.dot = u.state == UpdateStatus::available;
        safe->gear.repaint();
        safe->overlay.repaint();
    });
}

void MelodyCodeEditor::paint(juce::Graphics& g) { g.fillAll(Theme::bg); }

void MelodyCodeEditor::resized() {
    content.setBounds(0, 0, Theme::width, Theme::height);
    content.setTransform(juce::AffineTransform::scale((float) getWidth() / (float) Theme::width));
    layout();
    overlay.rescale(getLocalBounds(), (float) getWidth() / (float) Theme::width);
}

namespace L {
constexpr int pad = 24, headerH = 72, labelY = 90, selY = 108, rollY = 168, rollH = 372, ctlY = 576, ctlH = 80, ctl2Y = 668, ctl2H = 64, actY = 744;
}

void MelodyCodeEditor::layout() {
    const int W = Theme::width - 2 * L::pad;
    {
        const int gap = 12, w = (W - 4 * gap) / 5;
        int x = L::pad;
        for (auto* b : { &keyBox, &scaleBox, &styleBox, &vibeBox, &soundBox }) { b->setBounds(x, L::selY, w, AMUI::Size::field); x += w + gap; }
        // timbre arrows live inside the field, left of its chevron
        const auto r = soundBox.getBounds();
        soundNext.setBounds(r.getRight() - 40 - 28, r.getY() + 8, 28, 28);
        soundPrev.setBounds(r.getRight() - 40 - 58, r.getY() + 8, 28, 28);
    }
    roll.setBounds(L::pad, L::rollY, W, L::rollH);
    gear.setBounds(Theme::width - L::pad - 40, 16, 40, 40);
    {
        const int gap = 12, w5 = (W - 4 * gap) / 5, w4 = (W - 3 * gap) / 4;
        int x = L::pad;
        for (auto* s : { &role, &feel, &density, &range, &bars }) { s->setBounds(x + 12, L::ctlY + 32, w5 - 24, 36); x += w5 + gap; }
        x = L::pad;
        for (auto* s : { &bpm, &melVol, &chordVol, &bassVol }) { s->setBounds(x + 12, L::ctl2Y + 30, w4 - 24 - 40, 24); x += w4 + gap; }
    }
    {   // one action bar, same order as Chord Code; widths follow the labels, the spare room is shared out
        std::initializer_list<ActionButton*> bar { &play, &vari, &newMel, &gen, &historyBtn, &dragBtn, &dragWav, &save };
        const int gap = 10;
        std::vector<int> widths;
        int used = gap * ((int) bar.size() - 1);
        for (auto* b : bar) {
            const auto f = b->hero ? Theme::semi(15.0f).withExtraKerningFactor(AMUI::kerningHero) : Theme::button(13.0f);
            int w = (int) std::ceil(juce::GlyphArrangement::getStringWidth(f, b == &play ? juce::String("PARAR") : b->getButtonText())) + AMUI::Size::icon + 8 + 32;
            if (b->hero) w = juce::jmax(w, 190);
            widths.push_back(w); used += w;
        }
        const int extra = juce::jmax(0, W - used) / (int) bar.size();
        int x = L::pad, i = 0;
        for (auto* b : bar) {
            const int w = (i == (int) bar.size() - 1) ? L::pad + W - x : widths[(size_t) i] + extra;
            b->setBounds(x, L::actY, w, AMUI::Size::buttonLarge);
            x += w + gap; ++i;
        }
    }
}

void MelodyCodeEditor::paintContent(juce::Graphics& g) {
    using namespace Theme;
    auto all = juce::Rectangle<float>(0, 0, (float) width, (float) height);
    g.fillAll(bg);
    g.setGradientFill(juce::ColourGradient(acc.withAlpha(0.10f), 80, -80, acc.withAlpha(0.0f), 640, 300, true));
    g.fillRect(all);

    // header: mark + wordmark on the left, summary pill and settings on the right
    auto logo = juce::Rectangle<float>((float) L::pad, 18, 36, 36);
    g.setGradientFill(accent(logo));
    g.fillRoundedRectangle(logo, 10);
    drawIcon(g, Icon::note, juce::Rectangle<float>(22, 22).withCentre(logo.getCentre()), ink);
    const auto word = font(22.0f, true);
    g.setFont(word);
    g.setColour(fg);
    const float tx = logo.getRight() + 14;
    g.drawText("MELODY", juce::Rectangle<float>(tx, 18, 140, 36), juce::Justification::centredLeft);
    const float mw = juce::GlyphArrangement::getStringWidth(word, "MELODY ");
    auto codeR = juce::Rectangle<float>(tx + mw, 18, 100, 36);
    g.setGradientFill(juce::ColourGradient(acc, codeR.getX(), 0, acc2, codeR.getX() + 70, 0, false));
    g.drawText("CODE", codeR, juce::Justification::centredLeft);

    const auto& s = proc.settings();
    const auto dot = utf8("  \xc2\xb7  ");
    const auto shownPat = proc.pattern();
    const auto arrow = utf8(" \xe2\x86\x92 ");
    const auto padrao = utf8("PADR\xc3\x83O") + arrow;
    const juce::String info = (s.style == mc::Style::automatic ? padrao : juce::String()) + utf8(mc::styleName(shownPat.style))
                            + dot + (s.key < 0 || s.scale < 0 ? padrao : juce::String()) + juce::String(noteNames[shownPat.key]) + " " + utf8(ccp::scaleName(shownPat.scale))
                            + dot + utf8(mc::vibeName(s.vibe)) + dot + juce::String(s.bars) + " COMPASSOS" + dot + juce::String(proc.bpm) + " BPM";
    g.setFont(button(12.0f));
    const float iw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), info) + 44;
    auto pill = juce::Rectangle<float>((float) (width - L::pad) - 40 - 12 - iw, 18, iw, 36);
    g.setColour(panel); g.fillRoundedRectangle(pill, 18);
    g.setColour(line); g.drawRoundedRectangle(pill.reduced(0.5f), 18, 1);
    g.setColour(acc); g.fillEllipse(pill.getX() + 16, pill.getCentreY() - 3, 6, 6);
    g.setColour(mute); g.drawText(info, pill.withTrimmedLeft(30).withTrimmedRight(14), juce::Justification::centredLeft);
    g.setColour(line);
    g.fillRect(0.0f, (float) L::headerH, (float) width, 1.0f);

    auto caption = [&](const juce::String& t, float x, float y) {
        g.setColour(mute);
        g.setFont(label());
        g.drawText(t, juce::Rectangle<float>(x, y, 300, 14), juce::Justification::centredLeft);
    };
    caption("TOM", (float) keyBox.getX(), L::labelY); caption("ESCALA", (float) scaleBox.getX(), L::labelY); caption("ESTILO", (float) styleBox.getX(), L::labelY);
    caption("CLIMA", (float) vibeBox.getX(), L::labelY); caption("TIMBRE", (float) soundBox.getX(), L::labelY);

    const auto hintArea = juce::Rectangle<float>((float) L::pad, (float) (L::rollY + L::rollH + 8), 1000, 18);
    g.setColour(hint.isNotEmpty() ? acc : quiet.brighter(0.25f));
    g.setFont(font(12.0f));
    g.drawText(hint.isNotEmpty() ? hint
               : utf8("Clique no grid pra criar nota  \xc2\xb7  arraste pra mover  \xc2\xb7  puxe a ponta pra alongar  \xc2\xb7  duplo clique ou Delete apaga  \xc2\xb7  \xe2\x86\x91\xe2\x86\x93 muda a nota"),
               hintArea, juce::Justification::centredLeft);

    auto box = [&](juce::Rectangle<int> r, const juce::String& t, const juce::String& value) {
        g.setColour(panel);
        g.fillRoundedRectangle(r.toFloat(), AMUI::Size::radiusPanel);
        g.setColour(line);
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), AMUI::Size::radiusPanel, 1);
        caption(t, (float) r.getX() + 12, (float) r.getY() + 12);
        if (value.isNotEmpty()) {
            g.setColour(fg);
            g.setFont(semi(13.0f));
            g.drawText(value, juce::Rectangle<int>(r.getRight() - 52, r.getY() + 30, 40, 24).toFloat(), juce::Justification::centredRight);
        }
    };
    const int gap = 12, w4 = (width - 2 * L::pad - 3 * gap) / 4, w5 = (width - 2 * L::pad - 4 * gap) / 5;
    const char* titles[] { "FUN\xc3\x87\xc3\x83O", "LEVADA", "DENSIDADE", "REGISTRO", "COMPASSOS" };
    for (int i = 0; i < 5; ++i) box({ L::pad + i * (w5 + gap), L::ctlY, w5, L::ctlH }, utf8(titles[i]), {});
    box({ L::pad, L::ctl2Y, w4, L::ctl2H }, "BPM", juce::String(proc.bpm));
    box({ L::pad + (w4 + gap), L::ctl2Y, w4, L::ctl2H }, "MELODIA", juce::String((int) std::round(proc.melodyLevel * 100)));
    box({ L::pad + 2 * (w4 + gap), L::ctl2Y, w4, L::ctl2H }, "ACORDES", juce::String((int) std::round(proc.chordLevel * 100)));
    box({ L::pad + 3 * (w4 + gap), L::ctl2Y, w4, L::ctl2H }, "GRAVE", juce::String((int) std::round(proc.bassLevel * 100)));
}
