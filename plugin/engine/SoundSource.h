#pragma once
// JUCE-free. Common interface for all engine voices (wavetable/granular/spectral).

namespace soundx::engine {

class SoundSource {
public:
    virtual ~SoundSource() = default;
    virtual void setSampleRate(double sampleRate) = 0;
    virtual void noteOn(int midiNote, float velocity01) = 0;
    virtual void noteOff() = 0;
    virtual void kill() = 0;
    virtual bool isActive() const = 0;
    // Adds into dest; never allocates; numSamples bounded by caller.
    virtual void render(float* dest, int numSamples) = 0;
};

} // namespace soundx::engine
