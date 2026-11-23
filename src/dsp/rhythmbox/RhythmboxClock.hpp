// Pure C++20 DAW-syncable clock & transport telemetry types for KE Rhythmbox.
#pragma once

namespace ke310::rhythmbox
{

enum class ClockMode : int
{
    Internal = 0,  // Local TEMPO knob + local START/STOP
    TempoSync = 1, // Follows DAW BPM (when available), local START/STOP
    HostSync = 2   // Follows DAW Play/Stop + BPM + PPQ (falls back to Internal if no host transport)
};

struct HostTransportInfo
{
    bool hasTransport = false;
    bool isPlaying = false;
    bool hasBpm = false;
    double bpm = 106.8;
    bool hasPpqPosition = false;
    double ppqPosition = 0.0;
};

struct TempoLampState
{
    bool isRunning = false;
    bool beatFlash = false;
    bool isDownbeat = false;
    bool usingHostBpm = false;
    double beatPhase = 0.0;
    double effectiveBpm = 106.8;
    int currentStep = 0;
    int totalSteps = 64;
};

} // namespace ke310::rhythmbox
