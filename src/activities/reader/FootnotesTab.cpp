#include "FootnotesTab.h"

#include <EpdFontFamily.h>
#include <GfxRenderer.h>

#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr int LINE_HEIGHT = 40;
}

void FootnotesTab::onEnter() {
  selectedIndex = 0;
  updateRequired = true;
}

void FootnotesTab::loop() {
  bool needsRedraw = false;

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    if (selectedIndex > 0) {
      selectedIndex--;
      needsRedraw = true;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
    if (selectedIndex < footnotes.getCount() - 1) {
      selectedIndex++;
      needsRedraw = true;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    const FootnoteEntry* entry = footnotes.getEntry(selectedIndex);
    if (entry) {
      onSelectFootnote(entry->href);
    }
  }

  if (needsRedraw) {
    updateRequired = true;
  }
}

void FootnotesTab::render(int contentTop, int contentHeight) {
  const int marginLeft = 20;

  if (footnotes.getCount() == 0) {
    renderer.drawText(SMALL_FONT_ID, marginLeft, contentTop + 20, "No footnotes on this page");
    return;
  }

  for (int i = 0; i < footnotes.getCount(); i++) {
    const FootnoteEntry* entry = footnotes.getEntry(i);
    if (!entry) continue;

    const int y = contentTop + i * LINE_HEIGHT;

    if (i == selectedIndex) {
      renderer.drawText(UI_12_FONT_ID, marginLeft - 10, y, ">", EpdFontFamily::BOLD);
      renderer.drawText(UI_12_FONT_ID, marginLeft + 10, y, entry->number, EpdFontFamily::BOLD);
    } else {
      renderer.drawText(UI_12_FONT_ID, marginLeft + 10, y, entry->number);
    }
  }
}

int FootnotesTab::getCurrentPage() const { return 1; }
int FootnotesTab::getTotalPages() const { return 1; }
