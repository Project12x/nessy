#include "NsfPlayerWindow.h"
#include "BinaryData.h"

// ===========================================================================
// NsfPlayerView
// ===========================================================================

namespace {

// Channel colors (ARGB) and labels for the right-pane scope frames.
// Order: P1, P2, TRI, NSE, DMC — count matches NsfPlayerView::kNumScopes (5).
constexpr int          kNScopeCount = 5;
constexpr juce::uint32 kScopeColors[kNScopeCount] = {
    0xffcc0000,  // P1 red
    0xff336699,  // P2 blue
    0xff669933,  // TRI green
    0xffcc6600,  // NSE orange
    0xffc79a3a,  // DMC gold
};
constexpr const char*  kScopeLabel[kNScopeCount] = {
    "P1", "P2", "TRI", "NSE", "DMC"
};

} // namespace

NsfPlayerView::NsfPlayerView(NessyAudioProcessor& proc, NessyTheme theme)
    : m_proc(proc), m_theme(std::move(theme))
{
  // Controls styled by our LookAndFeel instance.
  m_loadBtn.setLookAndFeel(&m_lnf);
  m_playBtn.setLookAndFeel(&m_lnf);
  m_stopBtn.setLookAndFeel(&m_lnf);
  m_prevBtn.setLookAndFeel(&m_lnf);
  m_nextBtn.setLookAndFeel(&m_lnf);

  addAndMakeVisible(m_loadBtn);
  addAndMakeVisible(m_playBtn);
  addAndMakeVisible(m_stopBtn);
  addAndMakeVisible(m_prevBtn);
  addAndMakeVisible(m_nextBtn);

  m_loadBtn.onClick = [this] { loadNsfFile(); };
  m_playBtn.onClick = [this] { m_proc.setNsfPlaying(true);  repaint(); };
  m_stopBtn.onClick = [this] { m_proc.setNsfPlaying(false); repaint(); };
  m_prevBtn.onClick = [this] {
    int s = juce::jmax(0, m_proc.getNsfSong() - 1);
    m_proc.selectNsfSong(s);
    repaint();
  };
  m_nextBtn.onClick = [this] {
    int s = juce::jmin(m_proc.getNsfSongCount() - 1, m_proc.getNsfSong() + 1);
    m_proc.selectNsfSong(s);
    repaint();
  };

  // Scope components (right pane — un-fed for now; live traces are B3.3).
  for (int i = 0; i < kNScopeCount; ++i) {
    m_scopes[i] = std::make_unique<nessy::NessyScope>();
    m_scopes[i]->setTraceColour(juce::Colour(kScopeColors[i]));
    addAndMakeVisible(*m_scopes[i]);
  }

  applyThemeToControls();
  setSize(480, 320);
}

NsfPlayerView::~NsfPlayerView() {
  // Detach LookAndFeel before controls are destroyed.
  m_loadBtn.setLookAndFeel(nullptr);
  m_playBtn.setLookAndFeel(nullptr);
  m_stopBtn.setLookAndFeel(nullptr);
  m_prevBtn.setLookAndFeel(nullptr);
  m_nextBtn.setLookAndFeel(nullptr);
}

void NsfPlayerView::setTheme(NessyTheme t) {
  m_theme = std::move(t);
  applyThemeToControls();
  repaint();
}

void NsfPlayerView::applyThemeToControls() {
  const auto& t = m_theme;

  // TextButton colours: background = faceA/faceB range, text = nameText, stripe accent.
  auto styleBtn = [&](juce::TextButton& b, juce::Colour bg, juce::Colour fg) {
    b.setColour(juce::TextButton::buttonColourId,   bg);
    b.setColour(juce::TextButton::buttonOnColourId, bg.brighter(0.15f));
    b.setColour(juce::TextButton::textColourOffId,  fg);
    b.setColour(juce::TextButton::textColourOnId,   fg.brighter(0.3f));
    b.setColour(juce::ComboBox::outlineColourId,    t.faceBorder);
  };

  styleBtn(m_loadBtn, t.chipA,  t.railText);
  styleBtn(m_playBtn, t.trayA,  t.stripe);
  styleBtn(m_stopBtn, t.trayA,  t.hdrDim);
  styleBtn(m_prevBtn, t.chipA,  t.nameText);
  styleBtn(m_nextBtn, t.chipA,  t.nameText);
}

// ---------------------------------------------------------------------------
// paint — NES Front-Loader chrome
// ---------------------------------------------------------------------------
void NsfPlayerView::paint(juce::Graphics& g) {
  const auto& t  = m_theme;
  const auto  w  = getWidth();
  const auto  h  = getHeight();
  const int   divX = w * 5 / 8;  // left 5/8 = controls, right 3/8 = scopes

  // PCB background
  g.fillAll(juce::Colour(0xff14161f));

  // Header rail (top 28 px)
  auto hdr = juce::Rectangle<float>(0, 0, (float)w, 28.0f);
  nessy::vgrad(g, hdr, t.hdrA, t.hdrB, 0);
  // louver texture
  for (int y = 0; y < 28; y += 7) {
    g.setColour(juce::Colours::white.withAlpha(0.09f));
    g.fillRect(0, y, w, 1);
    g.setColour(juce::Colours::black.withAlpha(0.08f));
    g.fillRect(0, y + 1, w, 1);
  }
  // stripe accent
  g.setColour(t.stripe);
  g.fillRect(0, 25, w, 3);
  g.setColour(juce::Colours::black.withAlpha(0.55f));
  g.fillRect(0, 23, w, 2);

  // Title in header
  g.setColour(t.wordmark);
  g.setFont(nessy::pixelFont(9.0f));
  g.drawText("NESSY  NSF PLAYER", 10, 4, 260, 18, juce::Justification::centredLeft);

  // ---- Left pane panel ----
  auto leftPanel = juce::Rectangle<float>(8.0f, 36.0f, (float)(divX - 16), (float)(h - 44));
  nessy::bevelOut(g, leftPanel, t.faceA, t.faceB, 6.0f);
  auto lp = leftPanel.reduced(10.0f, 8.0f);

  // Metadata block (below the LOAD button — paint text labels)
  float metaY = lp.getY() + 44.0f;  // 36px reserved for LOAD button
  const float lineH = 14.0f;
  auto drawMeta = [&](const juce::String& label, const juce::String& val, float y) {
    g.setColour(t.nameText);
    g.setFont(nessy::pixelFont(5.0f));
    g.drawText(label, (int)lp.getX(), (int)y, (int)lp.getWidth(), (int)(lineH * 0.65f),
               juce::Justification::centredLeft);
    g.setColour(t.railText);
    g.setFont(nessy::pixelFont(6.5f));
    g.drawText(val.isEmpty() ? "-" : val,
               (int)lp.getX(), (int)(y + lineH * 0.65f), (int)lp.getWidth(), (int)(lineH * 0.75f),
               juce::Justification::centredLeft);
  };

  juce::String title = m_proc.getNsfTitle();
  if (title.length() > 20) title = title.substring(0, 19) + ".";
  drawMeta("TITLE",    title,                       metaY);
  drawMeta("ARTIST",   m_proc.getNsfArtist(),       metaY + lineH * 1.5f);
  drawMeta("(C)",      m_proc.getNsfCopyright(),    metaY + lineH * 3.0f);
  drawMeta("CHIPS",    m_proc.getNsfChips(),        metaY + lineH * 4.5f);

  // Transport row separator
  float transportY = metaY + lineH * 6.2f;
  g.setColour(t.faceBorder.withAlpha(0.4f));
  g.fillRect((int)lp.getX(), (int)transportY - 2, (int)lp.getWidth(), 1);

  // Song counter (between prev/next buttons — painted)
  {
    int total = juce::jmax(1, m_proc.getNsfSongCount());
    juce::String songLbl = "SONG "
                         + juce::String(m_proc.getNsfSong() + 1)
                         + "/" + juce::String(total);
    float btnW = 22.0f, gap = 4.0f;
    float songLabelX = lp.getX() + btnW + gap;
    float songLabelW = lp.getWidth() - 2 * (btnW + gap);
    float songLabelY = transportY + 4.0f;
    g.setColour(t.stripe);
    g.setFont(nessy::pixelFont(6.5f));
    g.drawText(songLbl, (int)songLabelX, (int)songLabelY,
               (int)songLabelW, 16, juce::Justification::centred);
  }

  // Playing indicator
  {
    bool playing = m_proc.isNsfPlaying() && m_proc.isNsfMode();
    nessy::led(g, {lp.getRight() - 8.0f, lp.getY() + 18.0f}, 5.0f,
               t.stripe, playing);
  }

  // ---- Right pane header ----
  auto rightHeader = juce::Rectangle<float>((float)divX + 4.0f, 36.0f,
                                             (float)(w - divX - 12), 14.0f);
  g.setColour(t.nameText);
  g.setFont(nessy::pixelFont(5.0f));
  g.drawText("CHANNEL SCOPES", rightHeader.toNearestInt(), juce::Justification::centredLeft);

  // ---- Scope frame backgrounds (recessed wells) ----
  // Actual NessyScope components are sized in resized(); here we paint the
  // surrounding bezel/cap row for each scope.
  auto rightArea = juce::Rectangle<int>(divX + 4, 52, w - divX - 12, h - 60);
  int scopeH = (rightArea.getHeight() - (kNScopeCount - 1) * 4) / kNScopeCount;
  for (int i = 0; i < kNScopeCount; ++i) {
    int scopeY = rightArea.getY() + i * (scopeH + 4);
    auto scopeRect = juce::Rectangle<int>(rightArea.getX(), scopeY,
                                          rightArea.getWidth(), scopeH);
    // bezel
    nessy::bevelIn(g, scopeRect.toFloat().expanded(1.5f),
                   juce::Colour(0xff0d0d0c), 3.0f);
    // colored channel cap label (left of scope)
    juce::Colour capCol = juce::Colour(kScopeColors[i]);
    g.setGradientFill({capCol.brighter(0.1f), (float)scopeRect.getX(), (float)scopeY,
                       capCol.darker(0.15f), (float)scopeRect.getX(),
                       (float)(scopeY + scopeH), false});
    g.fillRoundedRectangle((float)scopeRect.getX(), (float)scopeY, 9.0f, (float)scopeH, 2.0f);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(nessy::pixelFont(4.5f));
    g.drawText(kScopeLabel[i],
               scopeRect.getX(), scopeY, 12, scopeH,
               juce::Justification::centred);
  }
}

void NsfPlayerView::paintOverChildren(juce::Graphics& g) {
  const int divX = getWidth() * 5 / 8;
  auto rightArea = juce::Rectangle<int>(divX + 4, 52, getWidth() - divX - 12,
                                        getHeight() - 60);
  int scopeH = (rightArea.getHeight() - (kNScopeCount - 1) * 4) / kNScopeCount;
  for (int i = 0; i < kNScopeCount; ++i) {
    int scopeY = rightArea.getY() + i * (scopeH + 4);
    // The NessyScope child occupies the inner area; draw CRT glass over it.
    auto scopeRect = juce::Rectangle<float>(
        (float)(rightArea.getX() + 12), (float)scopeY,
        (float)(rightArea.getWidth() - 12), (float)scopeH);
    nessy::crtGlass(g, scopeRect);
  }
}

// ---------------------------------------------------------------------------
// resized — two-pane layout
// ---------------------------------------------------------------------------
void NsfPlayerView::resized() {
  const int w = getWidth(), h = getHeight();
  const int divX = w * 5 / 8;

  // Left pane inner area (mirror of paint lp rect)
  int lpX = 18, lpY = 44, lpW = divX - 26;

  // LOAD button
  m_loadBtn.setBounds(lpX, lpY, lpW, 28);

  // Transport buttons (below metadata — metaY + 6.2 * 14 = 44 + 86.8 ~ 131 from top of lp)
  int transportY = lpY + 44 + (int)(6.2f * 14.0f);
  int btnW = 22, gap = 4;
  m_prevBtn.setBounds(lpX, transportY + 4, btnW, 16);
  m_nextBtn.setBounds(lpX + lpW - btnW, transportY + 4, btnW, 16);

  // PLAY / STOP side by side below transport row
  int psY = transportY + 26;
  int psW = (lpW - gap) / 2;
  m_playBtn.setBounds(lpX, psY, psW, 22);
  m_stopBtn.setBounds(lpX + psW + gap, psY, psW, 22);

  // Right pane scopes (inner area, 12px cap on left for label)
  auto rightArea = juce::Rectangle<int>(divX + 4, 52, w - divX - 12, h - 60);
  int scopeH = (rightArea.getHeight() - (kNScopeCount - 1) * 4) / kNScopeCount;
  for (int i = 0; i < kNScopeCount; ++i) {
    int scopeY = rightArea.getY() + i * (scopeH + 4);
    m_scopes[i]->setBounds(rightArea.getX() + 12, scopeY,
                            rightArea.getWidth() - 12, scopeH);
  }
}

// ---------------------------------------------------------------------------
void NsfPlayerView::loadNsfFile() {
  m_chooser = std::make_unique<juce::FileChooser>(
      "Load NSF file", juce::File(), "*.nsf;*.nsfe");
  auto openFlags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;
  m_chooser->launchAsync(openFlags, [this](const juce::FileChooser& fc) {
    auto file = fc.getResult();
    if (file.existsAsFile()) {
      juce::MemoryBlock mb;
      if (file.loadFileAsData(mb)) {
        m_proc.loadNsf(static_cast<const uint8_t*>(mb.getData()),
                       mb.getSize(), 0);
      }
    }
    repaint();
  });
}

// ===========================================================================
// NsfPlayerWindow
// ===========================================================================
NsfPlayerWindow::NsfPlayerWindow(NessyAudioProcessor& proc, NessyTheme theme)
    : juce::DocumentWindow(
          "NSF PLAYER",
          juce::Colour(0xff14161f),
          juce::DocumentWindow::closeButton),
      m_proc(proc)
{
  // Build content view and hand ownership to the window.
  auto* view = new NsfPlayerView(proc, std::move(theme));
  m_view = view;
  setContentOwned(view, true);

  // Resizable, no corner-component (we use the OS resize handle).
  setResizable(true, false);
  setResizeLimits(400, 260, 900, 600);

  centreWithSize(480, 320);
  setUsingNativeTitleBar(false);
  setTitleBarTextCentred(true);
}

NsfPlayerWindow::~NsfPlayerWindow() = default;

void NsfPlayerWindow::setTheme(NessyTheme t) {
  if (m_view)
    m_view->setTheme(std::move(t));
}

void NsfPlayerWindow::refresh() {
  if (m_view)
    m_view->repaint();
}

void NsfPlayerWindow::closeButtonPressed() {
  // Hide the window and return to synth mode.
  setVisible(false);
  m_proc.setPlaybackMode(false);
}
