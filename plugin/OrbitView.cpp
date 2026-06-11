#include "OrbitView.h"
#include <cmath>
#include <complex>
#include <numbers>
#include "engine/Fft.h"
#include "engine/analysis/Consonance.h"

namespace {
constexpr auto kAccent = 0xff22d3ee;
constexpr auto kAccentB = 0xfff9a8d4;
constexpr auto kDim = 0xff0e3a40;
constexpr double kSampleRateAssumed = 44100.0; // close enough for display purposes
} // namespace

OrbitView::OrbitView(SoundXAudioProcessor& p) : processor_(p) {
    window_.assign(kFftSize, 0.0f);
    setInterceptsMouseClicks(false, false);
    startTimerHz(30);
}

OrbitView::~OrbitView() { stopTimer(); }

void OrbitView::timerCallback() {
    // pull whatever audio arrived; keep the most recent kFftSize samples
    float chunk[2048];
    for (;;) {
        const int got = processor_.popVisualizerSamples(chunk, 2048);
        if (got <= 0)
            break;
        if (got >= kFftSize) {
            std::copy(chunk + got - kFftSize, chunk + got, window_.begin());
            windowFill_ = kFftSize;
        } else {
            std::move(window_.begin() + got, window_.end(), window_.begin());
            std::copy(chunk, chunk + got, window_.end() - got);
            windowFill_ = std::min(kFftSize, windowFill_ + got);
        }
    }
    if (windowFill_ >= kFftSize)
        analyzeWindow();

    // animate: orbital speed scales with sqrt(ratio); global yaw drifts
    yaw_ += 0.012f;
    for (auto& b : bodies_)
        if (b.active)
            b.angle += 0.03f + 0.05f * std::sqrt(std::max(1.0f, b.ratio));
    repaint();
}

void OrbitView::analyzeWindow() {
    using namespace soundx::engine;

    float peakLevel = 0.0f;
    std::vector<std::complex<double>> spectrum(kFftSize);
    for (int i = 0; i < kFftSize; ++i) {
        const double hann = 0.5 * (1.0 - std::cos(2.0 * std::numbers::pi * double(i) / kFftSize));
        spectrum[size_t(i)] = {double(window_[size_t(i)]) * hann, 0.0};
        peakLevel = std::max(peakLevel, std::abs(window_[size_t(i)]));
    }
    level_ += (peakLevel - level_) * 0.4f;

    fft(spectrum);

    struct Peak { float freq, amp; };
    std::vector<Peak> peaks;
    for (int b = 2; b + 2 < kFftSize / 2; ++b) {
        const double m0 = std::abs(spectrum[size_t(b - 1)]);
        const double m1 = std::abs(spectrum[size_t(b)]);
        const double m2 = std::abs(spectrum[size_t(b + 1)]);
        if (m1 <= m0 || m1 < m2)
            continue;
        const double denom = m0 - 2.0 * m1 + m2;
        const double delta = denom != 0.0 ? 0.5 * (m0 - m2) / denom : 0.0;
        peaks.push_back({float((double(b) + delta) * kSampleRateAssumed / kFftSize),
                         float(4.0 * m1 / kFftSize)});
    }
    std::sort(peaks.begin(), peaks.end(), [](const Peak& a, const Peak& b) { return a.amp > b.amp; });
    if (peaks.size() > kMaxBodies)
        peaks.resize(kMaxBodies);

    // fundamental = lowest peak that is at least 10% of the strongest
    fundamentalHz_ = 0.0f;
    if (!peaks.empty()) {
        const float ref = peaks.front().amp * 0.1f;
        float lowest = 1.0e9f;
        for (const auto& p : peaks)
            if (p.amp >= ref && p.freq > 20.0f && p.freq < lowest)
                lowest = p.freq;
        if (lowest < 1.0e9f)
            fundamentalHz_ = lowest;
    }

    std::array<SpectralPartial, kMaxBodies> partials{};
    for (std::size_t i = 0; i < peaks.size(); ++i)
        partials[i] = {peaks[i].freq, std::min(1.0f, peaks[i].amp)};
    consonance_ = peaks.empty() ? 1.0f : consonanceScore01(partials.data(), peaks.size());

    for (int i = 0; i < kMaxBodies; ++i) {
        auto& body = bodies_[size_t(i)];
        if (std::size_t(i) >= peaks.size() || fundamentalHz_ <= 0.0f) {
            body.amp *= 0.8f; // fade out stale bodies
            body.active = body.amp > 0.01f;
            continue;
        }
        const float ratio = peaks[size_t(i)].freq / fundamentalHz_;
        body.ratio = ratio;
        body.amp = std::min(1.0f, peaks[size_t(i)].amp * 2.0f);
        const float nearestInt = std::round(ratio);
        body.inharmonicity = nearestInt > 0.0f
            ? std::min(0.5f, std::abs(ratio - nearestInt) / std::max(1.0f, nearestInt))
            : 0.5f;
        body.active = true;
    }

    // interval readout: strongest partial that isn't the fundamental
    intervalText_.clear();
    if (fundamentalHz_ > 0.0f)
        for (const auto& p : peaks) {
            const float ratio = p.freq / fundamentalHz_;
            if (ratio > 1.05f) {
                const auto info = soundx::engine::nearestInterval(ratio);
                intervalText_ = juce::String(info.name)
                              + (info.cents >= 0 ? " +" : " ")
                              + juce::String(info.cents, 1) + "c";
                break;
            }
        }
}

void OrbitView::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float maxR = std::min(bounds.getWidth(), bounds.getHeight()) * 0.42f;

    g.setColour(juce::Colour(kDim));
    g.drawRect(getLocalBounds(), 1);

    // 4D twist: morph rolls the projection plane
    const float roll = processor_.currentMorph() * float(std::numbers::pi) * 0.5f;
    const float cosRoll = std::cos(roll), sinRoll = std::sin(roll);
    const float tilt = 0.42f; // fixed camera tilt

    auto project = [&](float radius, float angle, float elevation) {
        // ring point in 3D, yawed, rolled, tilted, perspective-projected
        const float a = angle + yaw_;
        float x = radius * std::cos(a);
        float y = radius * std::sin(a);
        float z = elevation;
        const float y2 = y * cosRoll - z * sinRoll;
        const float z2 = y * sinRoll + z * cosRoll;
        const float yScreen = y2 * tilt;
        const float depth = 1.0f / (1.0f + 0.35f * (z2 / std::max(1.0f, maxR)));
        return juce::Point<float>(cx + x * depth, cy + yScreen * depth);
    };

    // orbit rings
    for (int ring = 1; ring <= 3; ++ring) {
        juce::Path path;
        const float radius = maxR * float(ring) / 3.0f;
        for (int step = 0; step <= 72; ++step) {
            const auto pt = project(radius, float(step) * float(std::numbers::pi) / 36.0f, 0.0f);
            if (step == 0)
                path.startNewSubPath(pt);
            else
                path.lineTo(pt);
        }
        path.closeSubPath();
        g.setColour(juce::Colour(kDim).withAlpha(0.8f));
        g.strokePath(path, juce::PathStrokeType(1.0f));
    }

    // core: glow scales with consonance and level
    const float coreR = 5.0f + 9.0f * level_;
    const auto coreColor = juce::Colour(kAccent);
    g.setColour(coreColor.withAlpha(0.15f + 0.3f * consonance_));
    g.fillEllipse(cx - coreR * 2.2f, cy - coreR * 2.2f, coreR * 4.4f, coreR * 4.4f);
    g.setColour(coreColor);
    g.fillEllipse(cx - coreR, cy - coreR, coreR * 2.0f, coreR * 2.0f);

    // bodies
    for (const auto& b : bodies_) {
        if (!b.active || b.ratio <= 0.0f)
            continue;
        const float radius = maxR * std::clamp(0.22f + 0.26f * std::log2(std::max(1.0f, b.ratio)),
                                               0.1f, 1.0f);
        const float elevation = maxR * 0.18f * std::sin(b.angle * 0.7f);
        const auto pt = project(radius, b.angle, elevation);
        const float size = 2.0f + 7.0f * b.amp;
        // harmonic partials stay cyan; inharmonic content shifts magenta
        const auto color = juce::Colour(kAccent).interpolatedWith(juce::Colour(kAccentB),
                                                                  b.inharmonicity * 2.0f);
        g.setColour(color.withAlpha(0.25f));
        g.fillEllipse(pt.x - size, pt.y - size, size * 2.0f, size * 2.0f);
        g.setColour(color);
        const float dot = size * 0.55f;
        g.fillEllipse(pt.x - dot, pt.y - dot, dot * 2.0f, dot * 2.0f);
    }

    // HUD readouts
    g.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(kAccent).withAlpha(0.85f));
    juce::String left = "LATENT FIELD";
    if (fundamentalHz_ > 0.0f)
        left += "  f0 " + juce::String(fundamentalHz_, 1) + " Hz";
    if (intervalText_.isNotEmpty())
        left += "  " + intervalText_;
    g.drawText(left, getLocalBounds().reduced(8).removeFromTop(14),
               juce::Justification::centredLeft);
    g.drawText("STABILITY " + juce::String(int(consonance_ * 100.0f)) + "%",
               getLocalBounds().reduced(8).removeFromTop(14), juce::Justification::centredRight);
}
