#include "Sampler.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <BinaryData.h>
#include <cmath>
#include <cstring>

struct Sampler::Zone {
    int rootCents {}, loopStart {}, loopEnd {};
    std::vector<float> data;   // mono, 44.1 kHz
};

namespace {
struct Entry { const char* inst; int rootCents, loopStart, loopEnd; const char* resource; };
const Entry table[] {
#include "SampleTable.inc"
};
// per instrument: where its lines should sit, the range single notes fold into, attack and release seconds, level
struct Shape { const char* name; int center, lowest, highest; float attack, release, level; };
const Shape shapes[] {
#include "InstrumentTable.inc"
};
constexpr int instCount = (int) (sizeof(shapes) / sizeof(shapes[0]));

int instIndex(const char* name) {
    for (int i = 0; i < instCount; ++i) if (std::strcmp(shapes[i].name, name) == 0) return i;
    jassertfalse;   // a kit names an instrument that tools/build_sets.py did not build
    return 0;
}

struct Bank {
    std::array<std::vector<Sampler::Zone*>, instCount> zones;
    std::vector<std::unique_ptr<Sampler::Zone>> owned;
    Bank() {
        juce::OggVorbisAudioFormat ogg;
        for (const auto& e : table) {
            int size = 0;
            const char* data = BinaryData::getNamedResource(e.resource, size);
            if (!data) continue;
            std::unique_ptr<juce::AudioFormatReader> reader(ogg.createReaderFor(new juce::MemoryInputStream(data, (size_t) size, false), true));
            if (!reader) continue;
            auto z = std::make_unique<Sampler::Zone>();
            z->rootCents = e.rootCents;
            juce::AudioBuffer<float> buf(1, (int) reader->lengthInSamples);
            reader->read(&buf, 0, buf.getNumSamples(), 0, true, false);
            z->data.assign(buf.getReadPointer(0), buf.getReadPointer(0) + buf.getNumSamples());
            z->loopEnd = juce::jmin(e.loopEnd, (int) z->data.size() - 1);
            z->loopStart = e.loopEnd > 0 && e.loopStart < z->loopEnd ? e.loopStart : 0;
            if (z->loopStart == 0) z->loopEnd = 0;
            zones[(size_t) instIndex(e.inst)].push_back(z.get());
            owned.push_back(std::move(z));
        }
    }
    const Sampler::Zone* find(int inst, int pitch) const {
        const Sampler::Zone* best = nullptr;
        int bestD = 1 << 30;
        for (auto* z : zones[(size_t) inst]) {
            // nearest root; a root just below beats one just above (shifting up sounds better)
            const int d = std::abs(z->rootCents - pitch * 100) * 4 + (z->rootCents > pitch * 100 ? 1 : 0);
            if (d < bestD) { bestD = d; best = z; }
        }
        return best;
    }
};
const Bank& bank() { static Bank b; return b; }

// Names and descriptions are UTF-8 (shown in Portuguese).
const Sampler::Kit kits[Sampler::kitCount] {
    { "CRISTAL",  "Sino de vidro na melodia, piano el\xc3\xa9trico nos acordes, sub redondo",          "bell_glass", "ep",         "sub_round" },
    { "BRISA",    "Flauta na melodia, piano nos acordes, sub quente",                                 "flute",      "piano",      "sub_warm" },
    { "SEDA",     "Marimba na melodia, violinos segurando os acordes, 808",                           "marimba",    "violin_ns",  "bass808", true },
    { "NINAR",    "Caixinha de m\xc3\xbasica na melodia, pluck macio nos acordes, sub quente",        "music_box",  "soft_pluck", "sub_warm" },
    { "NUVEM",    "Sino suave na melodia, pluck a\xc3\xa9reo segurando os acordes, 808",               "bell_soft",  "air_pluck",  "bass808", true },
    { "PIXEL",    "Bleep digital na melodia, stabs de synth nos acordes, 808",                        "bleep",      "stabs",      "bass808" },
    { "VELUDO",   "Timbre aveludado na melodia, piano el\xc3\xa9trico nos acordes, sub quente",        "velvet",     "ep",         "sub_warm" },
    { "\xc3\x89PICO", "Trompa na melodia, naipe de cordas nos acordes, tuba no grave",                  "horn",       "strings",    "tuba", true },
    { "F\xc3\x81" "BULA", "Clarinete na melodia, trompas nos acordes, contrabaixo pizzicato",            "clarinet",   "horn",       "contrabass", true },
    { "ENCANTO",  "Harpa na melodia, violinos pizzicato nos acordes, contrabaixo pizzicato",          "harp",       "pizzicato",  "contrabass" },
    { "PHONK",    "Cowbell na melodia, Rio Synth na fundamental dos acordes, Rio Bass",               "cowbell",    "rio_synth",  "bass_rio", false, true },
    { "BAILE",    "Violinos spiccato na melodia, trompas curtas nos acordes, grave de funk",          "spiccato",   "horn_stac",  "bass_grave2" },
    { "FLUXO",    "Bleep digital na melodia, stabs de synth nos acordes, baixo de funk",              "bleep",      "stabs",      "bass_anthem" },
};

// per set and layer (melody, bass, chords), in dB: measured with MC_DEMO_DIR so every set mixes like the demos
// Ed approved (chords about 4.5 dB under the melody, bass about 1 dB under) at about -15 LUFS
const float kitGainsDb[Sampler::kitCount][3] {
    { 10.2f, 12.4f,  5.3f },   // CRISTAL
    { 11.1f, 12.9f, 13.2f },   // BRISA
    {  9.5f, 13.2f, 11.7f },   // SEDA
    { 10.8f, 13.1f,  7.2f },   // NINAR
    {  9.6f, 14.4f, 10.1f },   // NUVEM
    { 11.1f, 13.4f,  5.2f },   // PIXEL
    { 11.3f, 12.9f,  5.2f },   // VELUDO
    {  6.6f,  8.9f,  6.5f },   // EPICO
    {  4.2f, 13.0f,  6.5f },   // FABULA
    {  9.6f, 14.0f,  7.2f },   // ENCANTO
    { 11.9f, 11.9f, 10.7f },   // PHONK
    {  9.5f, 12.2f,  7.0f },   // BAILE
    { 10.9f, 11.4f,  6.1f },   // FLUXO
};

struct KitInsts { int melody, chords, bass; };
const KitInsts& kitInsts(int k) {
    static const auto all = [] {
        std::array<KitInsts, Sampler::kitCount> a {};
        for (int i = 0; i < Sampler::kitCount; ++i) a[(size_t) i] = { instIndex(kits[i].melody), instIndex(kits[i].chords), instIndex(kits[i].bass) };
        return a;
    }();
    return all[(size_t) k];
}
}

const Sampler::Kit& Sampler::kit(int k) { return kits[k < 0 || k >= kitCount ? 0 : k]; }

void Sampler::prepare(double rate) {
    sr = rate;
    bank();   // decode once, off the audio thread
    for (auto& v : voices) v.active = false;
}

void Sampler::allNotesOff() {
    for (auto& v : voices) if (v.active) { v.releasing = true; v.rel = std::exp(-1.0f / (0.015f * (float) sr)); }
}

int Sampler::activeVoices() const {
    int n = 0;
    for (const auto& v : voices) n += v.active ? 1 : 0;
    return n;
}

void Sampler::noteOn(int pitch, int velocity, int lengthSamples, int layer) {
    const auto& K = kitInsts(kitIndex);
    const int inst = layer == 1 ? K.bass : layer == 2 ? K.chords : K.melody;
    const auto& S = shapes[inst];
    // the whole line moves by octaves toward the instrument's centre, then single notes fold into its range
    const int c = centers[(size_t) juce::jlimit(0, 2, layer)];
    if (c > 0) pitch += 12 * (int) std::lround((S.center - c) / 12.0);
    while (pitch < S.lowest) pitch += 12;
    while (pitch > S.highest) pitch -= 12;
    const float vel = juce::jlimit(0.0f, 1.0f, (float) velocity / 127.0f);
    const auto* z = bank().find(inst, pitch);
    if (!z || z->data.empty()) return;

    Voice* v = nullptr;
    for (auto& x : voices) if (!x.active) { v = &x; break; }
    if (!v) { v = &voices[0]; for (auto& x : voices) if (x.env < v->env) v = &x; }
    // a repeated note on the same layer lets the old one go quickly (a real player re-strikes)
    for (auto& x : voices)
        if (x.active && !x.releasing && x.layer == layer && x.zone == z && &x != v) { x.releasing = true; x.rel = std::exp(-1.0f / (0.05f * (float) sr)); }

    v->active = true; v->releasing = false; v->zone = z; v->layer = layer;
    v->pos = 0.0;
    v->inc = std::pow(2.0, (pitch * 100 - z->rootCents) / 1200.0) * 44100.0 / sr;
    v->gain = S.level * (0.25f + 0.75f * std::pow(vel, 1.4f)) * (layer == 1 ? bassLevel : melodyLevel) * 0.55f
            * juce::Decibels::decibelsToGain(kitGainsDb[kitIndex][juce::jlimit(0, 2, layer)]);
    v->env = S.attack < 0.005f ? 1.0f : 0.0f;
    v->atk = 1.0f / juce::jmax(1.0f, S.attack * (float) sr);
    v->rel = std::exp(-1.0f / (juce::jmax(0.005f, S.release) * (float) sr));
    v->left = juce::jmax(1, lengthSamples);
    // chords spread a little, melody centre, bass centre
    float pan = 0.0f;
    if (layer == 2) { panFlip ^= 1; pan = panFlip ? 0.25f : -0.25f; }
    v->panL = std::sqrt(0.5f * (1.0f - pan)); v->panR = std::sqrt(0.5f * (1.0f + pan));
}

void Sampler::render(float& l, float& r, float& dryL, float& dryR) {
    for (auto& v : voices) {
        if (!v.active) continue;
        const auto& d = v.zone->data;
        const int i = (int) v.pos;
        if (i + 1 >= (int) d.size()) { v.active = false; continue; }
        const float f = (float) (v.pos - i);
        float x = d[(size_t) i] + (d[(size_t) i + 1] - d[(size_t) i]) * f;
        v.pos += v.inc;
        if (v.zone->loopEnd > 0 && v.pos >= v.zone->loopEnd) v.pos -= v.zone->loopEnd - v.zone->loopStart;

        if (!v.releasing) {
            if (v.env < 1.0f) v.env = juce::jmin(1.0f, v.env + v.atk);
            if (--v.left <= 0) v.releasing = true;
        } else {
            v.env *= v.rel;
            if (v.env < 0.0005f) { v.active = false; continue; }
        }
        x *= v.env * v.gain;
        if (v.layer == 1) { dryL += x * v.panL; dryR += x * v.panR; }
        else { l += x * v.panL; r += x * v.panR; }
    }
}
