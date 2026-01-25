#include "EpubReaderFootnotesActivity.h"
#include <GfxRenderer.h>
#include "fontIds.h"
#include "MappedInputManager.h"

void EpubReaderFootnotesActivity::onEnter() {
  Activity::onEnter();
  renderScreen();
}

void EpubReaderFootnotesActivity::loop() {
  const int count = footnotes.size();

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (count > 0) {
      selectorIndex = (selectorIndex + count - 1) % count;
      renderScreen();
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (count > 0) {
      selectorIndex = (selectorIndex + 1) % count;
      renderScreen();
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (count > 0) {
      onSelectFootnote(footnotes[selectorIndex].href);
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
  }
}

void EpubReaderFootnotesActivity::renderScreen() {
  renderer.clearScreen();

  const int screenWidth = renderer.getScreenWidth();

  renderer.drawCenteredText(UI_12_FONT_ID, 60, "Footnotes", true, EpdFontFamily::BOLD);

  if (footnotes.empty()) {
    renderer.drawCenteredText(UI_10_FONT_ID, 200, "No footnotes on this page", true);
  } else {
    for (size_t i = 0; i < footnotes.size(); i++) {
      const int displayY = 120 + i * 40;
      const bool isSelected = (static_cast<int>(i) == selectorIndex);

      if (isSelected) {
        renderer.fillRect(0, displayY - 5, screenWidth, 35, true);
      }

      char label[128];
      snprintf(label, sizeof(label), "[%s] %s", footnotes[i].number, footnotes[i].href);
      std::string truncated = renderer.truncatedText(UI_10_FONT_ID, label, screenWidth - 40);

      renderer.drawText(UI_10_FONT_ID, 20, displayY + 10, truncated.c_str(), !isSelected);
    }
  }

  const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
