#include "ChaptersTab.h"

#include <GfxRenderer.h>

#include "KOReaderCredentialStore.h"
#include "MappedInputManager.h"
#include "fontIds.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
constexpr int LINE_HEIGHT = 30;
}  // namespace

void ChaptersTab::onEnter() {
  buildFilteredChapterList();

  selectorIndex = 0;
  for (size_t i = 0; i < filteredSpineIndices.size(); i++) {
    if (filteredSpineIndices[i] == currentSpineIndex) {
      selectorIndex = i;
      break;
    }
  }

  if (hasSyncOption()) {
    selectorIndex += 1;
  }
  updateRequired = true;
}

bool ChaptersTab::hasSyncOption() const { return KOREADER_STORE.hasCredentials(); }

int ChaptersTab::getTotalItems() const {
  const int syncCount = hasSyncOption() ? 2 : 0;
  return filteredSpineIndices.size() + syncCount;
}

bool ChaptersTab::isSyncItem(int index) const {
  if (!hasSyncOption()) return false;
  return index == 0 || index == getTotalItems() - 1;
}

int ChaptersTab::tocIndexFromItemIndex(int itemIndex) const {
  const int offset = hasSyncOption() ? 1 : 0;
  return itemIndex - offset;
}

int ChaptersTab::getPageItems(int contentTop, int contentHeight) const {
  int items = contentHeight / LINE_HEIGHT;
  return (items < 1) ? 1 : items;
}

void ChaptersTab::buildFilteredChapterList() {
  filteredSpineIndices.clear();
  for (int i = 0; i < epub->getSpineItemsCount(); i++) {
    if (epub->shouldHideFromToc(i)) continue;
    int tocIndex = epub->getTocIndexForSpineIndex(i);
    if (tocIndex == -1) continue;
    filteredSpineIndices.push_back(i);
  }
}

void ChaptersTab::loop() {
  const bool upReleased = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool downReleased = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int totalItems = getTotalItems();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (hasSyncOption() && (selectorIndex == 0 || selectorIndex == totalItems - 1)) {
      onLaunchSync();
      return;
    }

    int filteredIndex = selectorIndex;
    if (hasSyncOption()) filteredIndex -= 1;

    if (filteredIndex >= 0 && filteredIndex < static_cast<int>(filteredSpineIndices.size())) {
      onSelectSpineIndex(filteredSpineIndices[filteredIndex]);
    }
  } else if (upReleased) {
    if (totalItems > 0) {
      if (skipPage) {
        // This logic matches MyLibraryActivity
        // But for simplicity let's just do a page jump
      }
      selectorIndex = (selectorIndex + totalItems - 1) % totalItems;
      updateRequired = true;
    }
  } else if (downReleased) {
    if (totalItems > 0) {
      selectorIndex = (selectorIndex + 1) % totalItems;
      updateRequired = true;
    }
  }
}

void ChaptersTab::render(int contentTop, int contentHeight) {
  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems(contentTop, contentHeight);
  const int totalItems = getTotalItems();

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;
  renderer.fillRect(0, contentTop + (selectorIndex % pageItems) * LINE_HEIGHT - 2, pageWidth - 1, LINE_HEIGHT);

  for (int i = 0; i < pageItems; i++) {
    int itemIndex = pageStartIndex + i;
    if (itemIndex >= totalItems) break;

    const int displayY = contentTop + i * LINE_HEIGHT;
    const bool isSelected = (itemIndex == selectorIndex);

    if (isSyncItem(itemIndex)) {
      renderer.drawText(UI_10_FONT_ID, 20, displayY, ">> Sync Progress", !isSelected);
    } else {
      int filteredIndex = itemIndex;
      if (hasSyncOption()) filteredIndex -= 1;

      if (filteredIndex >= 0 && filteredIndex < static_cast<int>(filteredSpineIndices.size())) {
        int spineIndex = filteredSpineIndices[filteredIndex];
        int tocIndex = epub->getTocIndexForSpineIndex(spineIndex);

        if (tocIndex == -1) {
          renderer.drawText(UI_10_FONT_ID, 20, displayY, "Unnamed", !isSelected);
        } else {
          auto item = epub->getTocItem(tocIndex);
          const int indentSize = 20 + (item.level - 1) * 15;
          const std::string chapterName =
              renderer.truncatedText(UI_10_FONT_ID, item.title.c_str(), pageWidth - 40 - indentSize);
          renderer.drawText(UI_10_FONT_ID, indentSize, displayY, chapterName.c_str(), !isSelected);
        }
      }
    }
  }
}

int ChaptersTab::getCurrentPage() const {
  // We don't have enough context here to know pageItems easily without contentHeight
  // For now let's just return a placeholder or calculate it if we can.
  // Actually onEnter can't know the height either if it's dynamic.
  // Let's assume contentTop=60, contentHeight=screenHeight-120
  const int availableHeight = renderer.getScreenHeight() - 120;
  const int itemsPerPage = availableHeight / LINE_HEIGHT;
  return selectorIndex / (itemsPerPage > 0 ? itemsPerPage : 1) + 1;
}

int ChaptersTab::getTotalPages() const {
  const int availableHeight = renderer.getScreenHeight() - 120;
  const int itemsPerPage = availableHeight / LINE_HEIGHT;
  const int totalItems = getTotalItems();
  if (totalItems == 0) return 1;
  return (totalItems + itemsPerPage - 1) / (itemsPerPage > 0 ? itemsPerPage : 1);
}
