// © Kay Sievers <kay@vrfy.org>, 2020-2021
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <V2MIDI.h>

namespace V2Music {
class Scale {
public:
  enum Type {
    Chromatic,
    Major,
    Minor,
    HarmonicMinor,
    PentatonicMinor,
    BluesMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    PhrygianMajor,
    Lydian,
    Diminished,
    _count
  };

  // Return human readable name.
  static const char *getName(Type type);

  // The total number of notes used in the scale.
  static uint8_t getCount(Type type);

  // All scales start at C.
  static uint8_t getNote(Type type, uint8_t index);

  // Return if the given index of the note is used in this scale.
  static bool isUsed(Type type, uint8_t index);
};

class Keyboard {
public:
  // Get the key position in an octave, starting at zero.
  static uint8_t getKey(uint8_t note) {
    return note % 12;
  }

  // Get the octave number, starting at zero.
  static uint8_t getOctave(uint8_t note) {
    return note / 12;
  }

  // Return if the given note is a black piano key.
  static bool isBlackKey(uint8_t note) {
    return !Scale::isUsed(Scale::Major, getKey(note));
  }

  // Get the number of octaves and the fraction of the last octave where the key is located.
  static float getKeyOffset(uint8_t note) {
    // The center of the 12 keys of an octave in piano black/white layout. The black keys are
    // located in-between the white keys, but spread-out a bit towards the space where a black
    // keys is "missing". Only the G# key is right at the center between the white keys.
    constexpr float shift = 0.02;
    static constexpr float offset[12]{
      1.f * (1.f / 14.f),
      2.f * (1.f / 14.f) - shift,
      3.f * (1.f / 14.f),
      4.f * (1.f / 14.f) + shift,
      5.f * (1.f / 14.f),
      7.f * (1.f / 14.f),
      8.f * (1.f / 14.f) - shift,
      9.f * (1.f / 14.f),
      10.f * (1.f / 14.f), // centered
      11.f * (1.f / 14.f),
      12.f * (1.f / 14.f) + shift,
      13.f * (1.f / 14.f),
    };

    return (float)getOctave(note) + offset[getKey(note)];
  }

  // Get the distance of the given key, starting with the piano's first key A(-1).
  // The standard octave length is 16.5 cm.
  static float getKeyDistance(uint8_t note, float octave_width) {
    const float start = getKeyOffset(V2MIDI::A(-1));
    return (getKeyOffset(note) - start) * octave_width;
  }
};

class String {
public:
  // Get the fraction of the length of the vibrating string, for the given n-th
  // note above the base note of the string.
  static float getFraction(uint8_t note) {
    return powf(0.5, (float)note / 12.f);
  }

  // The the length to shorten the string by (the fret position from the shoulder)
  // for the n-th note above the base note of the string.
  static float getNoteDistance(uint8_t note, float length) {
    // Get the string length for the note.
    const float l = length * getFraction(note);

    // The distance from the end.
    return length - l;
  }
};

// A stack of currently playing notes with their velocity. A velocity of 0 will
// remove the note.
template <uint8_t size> class Playing {
public:
  void update(uint8_t note, uint8_t velocity) {
    // Remove note.
    for (uint8_t i = 0; i < _n; i++) {
      if (_list[i].note != note)
        continue;

      for (uint8_t k = i; k < _n - 1; k++)
        _list[k] = _list[k + 1];

      _n--;
      break;
    }

    if (velocity == 0)
      return;

    // Add note.
    _list[_n].note     = note;
    _list[_n].velocity = velocity;
    _n++;
  }

  bool getLast(uint8_t &note, uint8_t &velocity) {
    if (_n == 0)
      return false;

    note     = _list[_n - 1].note;
    velocity = _list[_n - 1].velocity;
    return true;
  }

  void reset() {
    _n = 0;
  }

private:
  struct {
    uint8_t note;
    uint8_t velocity;
  } _list[size]{};

  uint8_t _n = 0;
};

// Pressing 'Stop' in the audio workstation usually sends the control message AllNotesOff/AllSoundOff.
// Pressing it manually multiple times in a row, triggers a forced stop, disables all devices; it behaves
// like a panic button. Bursts of triggers in a very short time frame only, do not cause a forced stop.
// They might be sent by multiple active tracks, not by the user hitting 'Stop' repeatedly.
class ForcedStop {
public:
  void reset() {
    _state = State::Idle;
    _usec  = 0;
  }

  bool trigger() {
    switch (_state) {
      case State::Idle:
        _state = State::Trigger;
        _usec  = micros();
        break;

      case State::Trigger:
        // Do not trigger right away for bursts in a very short time frame.
        if ((unsigned long)(micros() - _usec) < 100 * 1000)
          break;

        // The triggers are too far apart from each other.
        if ((unsigned long)(micros() - _usec) > 500 * 1000) {
          reset();
          break;
        }

        // Repeated triggers over a longer period of time.
        reset();
        return true;
    }

    return false;
  }

private:
  enum State { Idle, Trigger } _state{};
  unsigned long _usec{};
};
};
