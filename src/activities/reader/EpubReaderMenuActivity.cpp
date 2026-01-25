#include "EpubReaderMenuActivity.h"
#include <GfxRenderer.h>
#include "fontIds.h"
#include "MappedInputManager.h"

void EpubReaderMenuActivity::onEnter() {
  Activity::onEnter();
  renderScreen();
}

void EpubReaderMenuActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    selectorIndex = (selectorIndex + 1) % 2;
    renderScreen();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    selectorIndex = (selectorIndex + 1) % 2;
    renderScreen();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    onSelectOption(static_cast<MenuOption>(selectorIndex));
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
  }
}

void EpubReaderMenuActivity::renderScreen() {
  renderer.clearScreen();

  const int screenWidth = renderer.getScreenWidth();

  renderer.drawCenteredText(UI_12_FONT_ID, 60, "Reader Menu", true, EpdFontFamily::BOLD);

  const char* options[] = {"Go to chapter", "View footnotes"};
  for (int i = 0; i < 2; i++) {
    const int displayY = 150 + i * 50;
    const bool isSelected = (i == selectorIndex);

    if (isSelected) {
        renderer.fillRect(0, displayY - 5, screenWidth, 40, true);
    }
    renderer.drawCenteredText(UI_12_FONT_ID, displayY + 10, options[i], !isSelected);
  }

  const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
