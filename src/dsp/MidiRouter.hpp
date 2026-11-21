// MIDI routing helper supporting user-selectable MIDI channels (1..16, Omni=0, Off=-1)
// for Upper / Lower / Pedal manuals, Multi-Channel and Single-Keyboard Split modes,
// per-manual octave shift, multi-manual layering, and stuck-note-free note-off tracking.
#pragma once

#include "KE310Engine.hpp"
#include "KE310Params.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace ke310::dsp
{

inline constexpr int kMidiChannelOff = -1;
inline constexpr int kMidiChannelOmni = 0;

enum class MidiMode : int
{
    MultiChannel = 0,
    SingleKeyboardSplit = 1
};

struct MidiRouterConfig
{
    MidiMode mode = MidiMode::MultiChannel;

    // User-selectable MIDI channels per manual:
    //   1..16 : MIDI Channel 1..16
    //   0     : Omni (responds to any MIDI channel 1..16)
    //  -1     : Off / Disabled
    int upperChannel = 1;
    int lowerChannel = 2;
    int pedalChannel = 3;

    // In SingleKeyboardSplit mode, accept either Omni (0) or a specific channel (1..16)
    int splitInputChannel = kMidiChannelOmni;

    // Per-manual octave transposition (-2 .. +2 octaves)
    int upperOctaveShift = 0;
    int lowerOctaveShift = 0;
    int pedalOctaveShift = 0;

    // Split points for SingleKeyboardSplit mode
    int pedalSplitMaxNote = 47; // B2; <= B2 -> Pedal
    int lowerSplitMaxNote = 59; // B3; C3..B3 -> Lower, >= C4 -> Upper

    // Pedal C2..C3 (36..48) octave folding & optional physical 49/13-key range clamping
    bool foldPedalOctave = true;
    bool clampToPhysicalKeyRange = false;
};

struct RoutedNote
{
    Manual manual = Manual::Upper;
    int midiNote = 60;
    bool valid = true;
};

struct RoutedNoteList
{
    std::array<RoutedNote, 3> items{};
    int count = 0;

    bool empty() const noexcept { return count == 0; }
};

class MidiRouter
{
public:
    void setConfig(const MidiRouterConfig& cfg) noexcept
    {
        cfg_ = cfg;
    }

    // Update configuration and safely release any active notes on manuals whose channel/mode changed
    void setConfig(const MidiRouterConfig& newCfg, KE310Engine& engine) noexcept
    {
        const bool modeChanged = (newCfg.mode != cfg_.mode);
        const bool upperChanged = modeChanged
            || (newCfg.upperChannel != cfg_.upperChannel)
            || (newCfg.upperOctaveShift != cfg_.upperOctaveShift);
        const bool lowerChanged = modeChanged
            || (newCfg.lowerChannel != cfg_.lowerChannel)
            || (newCfg.lowerOctaveShift != cfg_.lowerOctaveShift);
        const bool pedalChanged = modeChanged
            || (newCfg.pedalChannel != cfg_.pedalChannel)
            || (newCfg.pedalOctaveShift != cfg_.pedalOctaveShift)
            || (newCfg.foldPedalOctave != cfg_.foldPedalOctave);

        if (upperChanged || lowerChanged || pedalChanged)
        {
            releaseTrackedNotesForManuals(engine, upperChanged, lowerChanged, pedalChanged);
        }
        cfg_ = newCfg;
    }

    const MidiRouterConfig& config() const noexcept { return cfg_; }

    void reset() noexcept
    {
        for (auto& chArr : activeSlots_)
        {
            for (auto& slot : chArr)
            {
                slot.count = 0;
            }
        }
    }

    // Route an incoming (channel1Based, midiNote) to 0..3 target manuals
    RoutedNoteList routeNote(int channel1Based, int midiNote) const noexcept
    {
        RoutedNoteList out{};
        if (channel1Based < 1 || channel1Based > 16 || midiNote < 0 || midiNote > 127)
        {
            return out;
        }

        if (cfg_.mode == MidiMode::SingleKeyboardSplit)
        {
            if (!channelMatches(cfg_.splitInputChannel, channel1Based))
            {
                return out;
            }
            if (midiNote <= cfg_.pedalSplitMaxNote)
            {
                appendTarget(out, Manual::Pedal, midiNote);
            }
            else if (midiNote <= cfg_.lowerSplitMaxNote)
            {
                appendTarget(out, Manual::Lower, midiNote);
            }
            else
            {
                appendTarget(out, Manual::Upper, midiNote);
            }
            return out;
        }

        // MultiChannel mode: check each manual's user-configured channel independently
        // (supports distinct channels, Omni, Off, or multi-manual layering on the same channel)
        if (channelMatches(cfg_.pedalChannel, channel1Based))
        {
            appendTarget(out, Manual::Pedal, midiNote);
        }
        if (channelMatches(cfg_.lowerChannel, channel1Based))
        {
            appendTarget(out, Manual::Lower, midiNote);
        }
        if (channelMatches(cfg_.upperChannel, channel1Based))
        {
            appendTarget(out, Manual::Upper, midiNote);
        }
        return out;
    }

    // Primary single-target route convenience method (sets valid=false if channel is unassigned)
    RoutedNote route(int channel1Based, int midiNote) const noexcept
    {
        const auto list = routeNote(channel1Based, midiNote);
        if (list.count > 0)
        {
            return list.items[0];
        }
        return RoutedNote{Manual::Upper, std::clamp(midiNote, 0, 127), false};
    }

    // Dispatch Note-On to KE310Engine and remember exact target manual(s)/note(s) for Note-Off
    void handleNoteOn(int channel1Based, int rawMidiNote, KE310Engine& engine) noexcept
    {
        if (channel1Based < 1 || channel1Based > 16 || rawMidiNote < 0 || rawMidiNote > 127)
        {
            return;
        }

        // If the same (channel, key) was already held, release its previous targets first
        handleNoteOff(channel1Based, rawMidiNote, engine);

        const auto routed = routeNote(channel1Based, rawMidiNote);
        if (routed.empty())
        {
            return;
        }

        auto& slot = activeSlots_[static_cast<std::size_t>(channel1Based - 1)][static_cast<std::size_t>(rawMidiNote)];
        slot.count = static_cast<std::uint8_t>(routed.count);
        for (int i = 0; i < routed.count; ++i)
        {
            slot.manuals[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(routed.items[static_cast<std::size_t>(i)].manual);
            slot.notes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(routed.items[static_cast<std::size_t>(i)].midiNote);
            engine.noteOn(routed.items[static_cast<std::size_t>(i)].manual,
                          routed.items[static_cast<std::size_t>(i)].midiNote);
        }
    }

    // Dispatch Note-Off using the exact manual(s)/note(s) recorded at Note-On time
    void handleNoteOff(int channel1Based, int rawMidiNote, KE310Engine& engine) noexcept
    {
        if (channel1Based < 1 || channel1Based > 16 || rawMidiNote < 0 || rawMidiNote > 127)
        {
            return;
        }

        auto& slot = activeSlots_[static_cast<std::size_t>(channel1Based - 1)][static_cast<std::size_t>(rawMidiNote)];
        if (slot.count > 0)
        {
            for (std::uint8_t i = 0; i < slot.count; ++i)
            {
                engine.noteOff(static_cast<Manual>(slot.manuals[i]), static_cast<int>(slot.notes[i]));
            }
            slot.count = 0;
            return;
        }

        // Fallback if noteOff arrived without a tracked noteOn
        const auto routed = routeNote(channel1Based, rawMidiNote);
        for (int i = 0; i < routed.count; ++i)
        {
            engine.noteOff(routed.items[static_cast<std::size_t>(i)].manual,
                           routed.items[static_cast<std::size_t>(i)].midiNote);
        }
    }

    void handleAllNotesOff(KE310Engine& engine) noexcept
    {
        reset();
        engine.allNotesOff();
    }

private:
    struct ActiveSlot
    {
        std::uint8_t count = 0;
        std::array<std::uint8_t, 3> manuals{};
        std::array<std::uint8_t, 3> notes{};
    };

    static bool channelMatches(int assignedChannel, int incomingChannel1Based) noexcept
    {
        if (assignedChannel == kMidiChannelOff)
        {
            return false;
        }
        if (assignedChannel == kMidiChannelOmni)
        {
            return (incomingChannel1Based >= 1 && incomingChannel1Based <= 16);
        }
        return assignedChannel == incomingChannel1Based;
    }

    void appendTarget(RoutedNoteList& out, Manual manual, int rawNote) const noexcept
    {
        if (out.count >= 3)
        {
            return;
        }

        int mapped = rawNote;
        if (manual == Manual::Upper)
        {
            mapped = std::clamp(rawNote + 12 * std::clamp(cfg_.upperOctaveShift, -4, 4), 0, 127);
            if (cfg_.clampToPhysicalKeyRange && (mapped < 48 || mapped > 96))
            {
                return;
            }
        }
        else if (manual == Manual::Lower)
        {
            mapped = std::clamp(rawNote + 12 * std::clamp(cfg_.lowerOctaveShift, -4, 4), 0, 127);
            if (cfg_.clampToPhysicalKeyRange && (mapped < 36 || mapped > 84))
            {
                return;
            }
        }
        else
        {
            mapped = std::clamp(rawNote + 12 * std::clamp(cfg_.pedalOctaveShift, -4, 4), 0, 127);
            if (cfg_.clampToPhysicalKeyRange && !cfg_.foldPedalOctave && (mapped < 36 || mapped > 48))
            {
                return;
            }
            mapped = foldToPedalRange(mapped);
        }

        out.items[static_cast<std::size_t>(out.count)] = RoutedNote{manual, mapped, true};
        ++out.count;
    }

    int foldToPedalRange(int note) const noexcept
    {
        if (!cfg_.foldPedalOctave)
        {
            return std::clamp(note, 36, 48);
        }
        int n = note;
        while (n < 36)
        {
            n += 12;
        }
        while (n > 48)
        {
            n -= 12;
        }
        return n;
    }

    void releaseTrackedNotesForManuals(KE310Engine& engine,
                                       bool releaseUpper, bool releaseLower, bool releasePedal) noexcept
    {
        for (auto& chArr : activeSlots_)
        {
            for (auto& slot : chArr)
            {
                if (slot.count == 0)
                {
                    continue;
                }
                std::uint8_t writeIdx = 0;
                for (std::uint8_t i = 0; i < slot.count; ++i)
                {
                    const auto man = static_cast<Manual>(slot.manuals[i]);
                    const bool shouldRel = (man == Manual::Upper && releaseUpper)
                                        || (man == Manual::Lower && releaseLower)
                                        || (man == Manual::Pedal && releasePedal);
                    if (shouldRel)
                    {
                        engine.noteOff(man, static_cast<int>(slot.notes[i]));
                    }
                    else
                    {
                        slot.manuals[writeIdx] = slot.manuals[i];
                        slot.notes[writeIdx] = slot.notes[i];
                        ++writeIdx;
                    }
                }
                slot.count = writeIdx;
            }
        }
    }

    MidiRouterConfig cfg_{};
    std::array<std::array<ActiveSlot, 128>, 16> activeSlots_{};
};

} // namespace ke310::dsp
