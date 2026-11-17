#pragma once
#include <array>
#include <vector>

// Sampled instruments, grouped in sound sets ("conjuntos"). A set gives each layer (melody, chords, bass)
// its own instrument; tools/build_sets.py prepares the samples (recorded notes, the VSCO orchestra and
// synthesized timbres stored as samples) and embeds them in the binary.
class Sampler {
public:
    struct Kit {
        const char* name; const char* description;
        const char* melody; const char* chords; const char* bass;   // instrument names (InstrumentTable.inc)
        bool sustainChords { false };   // bowed / airy chord sounds: one held chord per change instead of the style's rhythm
        bool rootChords { false };      // stab one-shots tuned to a chord: play only each chord's root, in the style's rhythm
        int synth { -1 };               // >= 0: a synthesized kit played by Synth with that Tone preset (none at the moment)
    };
    static constexpr int kitCount = 13;
    static const Kit& kit(int);

    void prepare(double sampleRate);
    void setKit(int k) { kitIndex = k < 0 || k >= kitCount ? 0 : k; }
    void setLevels(float bass, float melody) { bassLevel = bass; melodyLevel = melody; }
    // Median pitch of each layer of the playing pattern (melody, bass, chords): every line is moved by whole
    // octaves so it sits where its instrument sounds best. 0 = unknown, no move.
    void setCenters(int melody, int bass, int chords) { centers = { melody, bass, chords }; }
    // layer: 0 melody, 1 bass, 2 chords
    void noteOn(int pitch, int velocity, int lengthSamples, int layer);
    void allNotesOff();
    // adds one sample: melody and chords to l/r (they go through the Vibe's effects), the bass to the dry pair
    void render(float& l, float& r, float& dryL, float& dryR);
    int activeVoices() const;

    struct Zone;

private:
    struct Voice {
        bool active {}, releasing {};
        const Zone* zone {};
        double pos {}, inc {};
        float gain {}, env {}, atk {}, rel {}, panL {}, panR {};
        int left {}, layer {};
    };
    std::array<Voice, 64> voices {};
    double sr { 44100.0 };
    int kitIndex {};
    float bassLevel { 0.8f }, melodyLevel { 0.8f };
    std::array<int, 3> centers {};
    int panFlip {};
};
