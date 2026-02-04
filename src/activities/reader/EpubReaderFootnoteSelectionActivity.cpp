#include "EpubReaderFootnoteSelectionActivity.h"

#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr int CONTENT_START_Y = 60;
constexpr int FOOTNOTE_LINE_HEIGHT = 40;
}  // namespace

void EpubReaderFootnoteSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<EpubReaderFootnoteSelectionActivity*>(param);
  self->displayTaskLoop();
}

void EpubReaderFootnoteSelectionActivity::onEnter() {
  ActivityWithSubactivity::onEnter();
  renderingMutex = xSemaphoreCreateMutex();
  selectedIndex = 0;
  updateRequired = true;
  xTaskCreate(&EpubReaderFootnoteSelectionActivity::taskTrampoline, "EpubFootnoteSelectTask", 4096, this, 1,
              &displayTaskHandle);
}

void EpubReaderFootnoteSelectionActivity::onExit() {
  ActivityWithSubactivity::onExit();
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void EpubReaderFootnoteSelectionActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
    return;
  }

  bool needsRedraw = false;
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
      mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (footnotes.getCount() > 0) {
      selectedIndex = (selectedIndex + footnotes.getCount() - 1) % footnotes.getCount();
      needsRedraw = true;
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
             mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (footnotes.getCount() > 0) {
      selectedIndex = (selectedIndex + 1) % footnotes.getCount();
      needsRedraw = true;
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (footnotes.getCount() > 0) {
      const FootnoteEntry* entry = footnotes.getEntry(selectedIndex);
      if (entry) {
        onSelectFootnote(entry->href);
      }
    }
  }

  if (needsRedraw) {
    updateRequired = true;
  }
}

void EpubReaderFootnoteSelectionActivity::displayTaskLoop() {
  while (true) {
    if (updateRequired && !subActivity) {
      updateRequired = false;
      xSemaphoreTake(renderingMutex, portMAX_DELAY);
      renderScreen();
      xSemaphoreGive(renderingMutex);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void EpubReaderFootnoteSelectionActivity::renderScreen() {
  renderer.clearScreen();
  const auto pageWidth = renderer.getScreenWidth();

  renderer.drawCenteredText(UI_12_FONT_ID, 15, "Footnotes", true, EpdFontFamily::BOLD);

  const int marginLeft = 20;
  if (footnotes.getCount() == 0) {
    renderer.drawText(SMALL_FONT_ID, marginLeft, CONTENT_START_Y + 20, "No footnotes on this page");
  } else {
    for (int i = 0; i < footnotes.getCount(); i++) {
      const FootnoteEntry* entry = footnotes.getEntry(i);
      if (!entry) continue;
      const int y = CONTENT_START_Y + i * FOOTNOTE_LINE_HEIGHT;
      if (i == selectedIndex) {
        renderer.fillRect(0, y - 5, pageWidth - 1, FOOTNOTE_LINE_HEIGHT, true);
        renderer.drawText(UI_12_FONT_ID, marginLeft + 10, y, entry->number, EpdFontFamily::BOLD, false);
      } else {
        renderer.drawText(UI_12_FONT_ID, marginLeft + 10, y, entry->number);
      }
    }
  }

  const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
