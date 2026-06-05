#include "Arpeggiator.h"

void Arpeggiator::noteOn(int midiNote) {
  if (midiNote < 0 || midiNote > 127)
    return;

  // Check if already held
  for (int i = 0; i < heldCount_; ++i) {
    if (heldNotes_[i] == midiNote)
      return;
  }

  if (heldCount_ < static_cast<int>(heldNotes_.size())) {
    heldNotes_[heldCount_++] = midiNote;
    rebuildSequence();
  }
}

void Arpeggiator::noteOff(int midiNote) {
  for (int i = 0; i < heldCount_; ++i) {
    if (heldNotes_[i] == midiNote) {
      heldNotes_[i] = heldNotes_[--heldCount_];
      rebuildSequence();
      return;
    }
  }
}

int Arpeggiator::tick() {
  if (!enabled_ || seqCount_ == 0)
    return -1;

  int note = sequence_[seqIndex_];
  seqIndex_ = (seqIndex_ + 1) % seqCount_;

  // Reshuffle on wrap for Random mode
  if (pattern_ == Pattern::Random && seqIndex_ == 0) {
    std::shuffle(sequence_.begin(), sequence_.begin() + seqCount_, rng_);
  }

  lastNote_ = note;
  return note;
}

void Arpeggiator::reset() {
  heldCount_ = 0;
  seqCount_ = 0;
  seqIndex_ = 0;
  lastNote_ = -1;
}

// Adapted from Breadbin SID synth rebuildArpSequence()
void Arpeggiator::rebuildSequence() {
  seqCount_ = 0;
  if (heldCount_ == 0)
    return;

  // Sort held notes into work buffer
  for (int i = 0; i < heldCount_; ++i)
    sortBuf_[i] = heldNotes_[i];
  std::sort(sortBuf_.begin(), sortBuf_.begin() + heldCount_);

  // Build base sequence with octave expansion
  int baseCount = 0;
  for (int oct = 0; oct < octaves_; ++oct) {
    for (int i = 0; i < heldCount_; ++i) {
      int transposed = sortBuf_[i] + (oct * 12);
      if (transposed <= 127 && baseCount < static_cast<int>(baseBuf_.size())) {
        baseBuf_[baseCount++] = transposed;
      }
    }
  }

  // Apply pattern
  switch (pattern_) {
  case Pattern::Up:
    for (int i = 0; i < baseCount; ++i)
      sequence_[i] = baseBuf_[i];
    seqCount_ = baseCount;
    break;

  case Pattern::Down:
    for (int i = 0; i < baseCount; ++i)
      sequence_[i] = baseBuf_[baseCount - 1 - i];
    seqCount_ = baseCount;
    break;

  case Pattern::UpDown:
    for (int i = 0; i < baseCount; ++i)
      sequence_[i] = baseBuf_[i];
    seqCount_ = baseCount;
    // Descending pass excluding endpoints (avoids double-hitting top/bottom)
    if (baseCount > 1) {
      for (int i = baseCount - 2; i > 0; --i) {
        if (seqCount_ < static_cast<int>(sequence_.size()))
          sequence_[seqCount_++] = baseBuf_[i];
      }
    }
    break;

  case Pattern::Random:
    for (int i = 0; i < baseCount; ++i)
      sequence_[i] = baseBuf_[i];
    seqCount_ = baseCount;
    std::shuffle(sequence_.begin(), sequence_.begin() + seqCount_, rng_);
    break;
  }

  // Reset index if out of bounds
  if (seqIndex_ >= seqCount_)
    seqIndex_ = 0;
}
