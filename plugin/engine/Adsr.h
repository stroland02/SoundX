#pragma once
// JUCE-free linear ADSR. nextLevel() is RT-safe.
#include <algorithm>

namespace soundx::engine {

class Adsr {
public:
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }

    void setParams(float attackSeconds, float decaySeconds, float sustain01, float releaseSeconds) {
        attackS_ = std::max(attackSeconds, 0.0f);
        decayS_ = std::max(decaySeconds, 0.0f);
        sustain_ = std::clamp(sustain01, 0.0f, 1.0f);
        releaseS_ = std::max(releaseSeconds, 0.0f);
    }

    void noteOn() { stage_ = Stage::Attack; }

    void noteOff() {
        if (stage_ != Stage::Idle) {
            releaseStart_ = level_;
            stage_ = Stage::Release;
        }
    }

    void reset() {
        stage_ = Stage::Idle;
        level_ = 0.0f;
    }

    bool isActive() const { return stage_ != Stage::Idle; }

    float nextLevel() noexcept {
        switch (stage_) {
        case Stage::Attack:
            level_ += perSample(attackS_); // 0 -> 1 in attackS_
            if (level_ >= 1.0f) {
                level_ = 1.0f;
                stage_ = Stage::Decay;
            }
            break;
        case Stage::Decay:
            level_ -= perSample(decayS_) * (1.0f - sustain_); // 1 -> sustain in decayS_
            if (level_ <= sustain_) {
                level_ = sustain_;
                stage_ = Stage::Sustain;
            }
            break;
        case Stage::Sustain:
            level_ = sustain_;
            break;
        case Stage::Release:
            level_ -= perSample(releaseS_) * releaseStart_; // releaseStart_ -> 0 in releaseS_
            if (level_ <= 0.0f) {
                level_ = 0.0f;
                stage_ = Stage::Idle;
            }
            break;
        case Stage::Idle:
            level_ = 0.0f;
            break;
        }
        return level_;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    // amount of a 0->1 ramp covered per sample for a stage lasting `seconds`
    float perSample(float seconds) const {
        return (seconds <= 0.0f || sampleRate_ <= 0.0) ? 1.0f : float(1.0 / (sampleRate_ * seconds));
    }

    Stage stage_ = Stage::Idle;
    double sampleRate_ = 0.0;
    float attackS_ = 0.01f, decayS_ = 0.1f, sustain_ = 0.8f, releaseS_ = 0.2f;
    float level_ = 0.0f;
    float releaseStart_ = 0.0f;
};

} // namespace soundx::engine
