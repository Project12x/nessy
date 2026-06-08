#pragma once

// NsfPlayerWindow — themed two-pane NSF player floating window.
// Phase B.3: DocumentWindow matching the main Front-Loader NES skin.
//
// NsfPlayerView  — content Component (left pane: controls + metadata;
//                                     right pane: framed CRT scope placeholders).
// NsfPlayerWindow — DocumentWindow that owns an NsfPlayerView.
//
// GPL-3.0 (part of the Nessy plugin).

#include "PluginEditor.h"   // NessyTheme
#include "NessyUI.h"        // nessy::NessyLookAndFeel, nessy::NessyScope, nessy::pixelFont, chrome helpers
#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <array>

// ===========================================================================
// NsfPlayerView — two-pane content component
// ===========================================================================
class NsfPlayerView : public juce::Component {
public:
  explicit NsfPlayerView(NessyAudioProcessor& proc, NessyTheme theme);
  ~NsfPlayerView() override;

  void setTheme(NessyTheme t);

  void paint(juce::Graphics&) override;
  void paintOverChildren(juce::Graphics&) override;
  void resized() override;

  // Number of channel scope frames in the right pane.
  static constexpr int kNumScopes = 5;

private:
  void loadNsfFile();
  void applyThemeToControls();

  NessyAudioProcessor&    m_proc;
  NessyTheme              m_theme;
  nessy::NessyLookAndFeel m_lnf;

  // Left pane controls
  juce::TextButton m_loadBtn { "LOAD NSF" };
  juce::TextButton m_playBtn { "PLAY" };
  juce::TextButton m_stopBtn { "STOP" };
  juce::TextButton m_prevBtn { "<" };
  juce::TextButton m_nextBtn { ">" };

  // File chooser (must outlive the async callback)
  std::unique_ptr<juce::FileChooser> m_chooser;

  // Right pane scope components (un-fed for now; live traces are B3.3)
  std::array<std::unique_ptr<nessy::NessyScope>, kNumScopes> m_scopes;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NsfPlayerView)
};

// ===========================================================================
// NsfPlayerWindow — DocumentWindow shell
// ===========================================================================
class NsfPlayerWindow : public juce::DocumentWindow {
public:
  NsfPlayerWindow(NessyAudioProcessor& proc, NessyTheme theme);
  ~NsfPlayerWindow() override;

  void setTheme(NessyTheme t);

  // Update metadata text and repaint (call from editor's timerCallback).
  void refresh();

  void closeButtonPressed() override;

private:
  NessyAudioProcessor& m_proc;
  NsfPlayerView*       m_view = nullptr;  // owned by DocumentWindow via setContentOwned

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NsfPlayerWindow)
};
