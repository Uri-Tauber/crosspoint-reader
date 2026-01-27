#include "EpubReaderNavigationActivity.h"

#include <GfxRenderer.h>

#include "KOReaderCredentialStore.h"
#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "ScreenComponents.h"
#include "fontIds.h"

namespace {
// Time threshold for treating a long press as a page-up/page-down
constexpr int SKIP_PAGE_MS = 700;

// Layout constants for tabs and content
constexpr int TAB_BAR_Y = 15;
constexpr int CONTENT_START_Y = 60;
constexpr int LINE_HEIGHT = 30;
constexpr int LEFT_MARGIN = 20;
constexpr int RIGHT_MARGIN = 40;  // Extra space for scroll indicator
}  // namespace

bool EpubReaderNavigationActivity::hasSyncOption() const { return KOREADER_STORE.hasCredentials(); }

int EpubReaderNavigationActivity::getTotalItems() const {
  if (currentTab == NavTab::Chapters) {
    // Add 2 for sync options (top and bottom) if credentials are configured
    const int syncCount = hasSyncOption() ? 2 : 0;
    return static_cast<int>(filteredSpineIndices.size()) + syncCount;
  } else {
    return footnotes.getCount();
  }
}

bool EpubReaderNavigationActivity::isSyncItem(int index) const {
  if (currentTab != NavTab::Chapters || !hasSyncOption()) return false;
  // First item and last item are sync options
  return index == 0 || index == getTotalItems() - 1;
}

int EpubReaderNavigationActivity::tocIndexFromItemIndex(int itemIndex) const {
  // Account for the sync option at the top
  const int offset = hasSyncOption() ? 1 : 0;
  return itemIndex - offset;
}

int EpubReaderNavigationActivity::getPageItems() const {
  const int screenHeight = renderer.getScreenHeight();
  const int bottomBarHeight = 60;  // Space for button hints
  const int availableHeight = screenHeight - CONTENT_START_Y - bottomBarHeight;
  int items = availableHeight / LINE_HEIGHT;
  if (items < 1) {
    items = 1;
  }
  return items;
}

int EpubReaderNavigationActivity::getTotalPages() const {
  const int itemCount = getTotalItems();
  const int pageItems = getPageItems();
  if (itemCount == 0) return 1;
  return (itemCount + pageItems - 1) / pageItems;
}

int EpubReaderNavigationActivity::getCurrentPage() const {
  const int pageItems = getPageItems();
  return selectorIndex / pageItems + 1;
}

void EpubReaderNavigationActivity::taskTrampoline(void* param) {
  auto* self = static_cast<EpubReaderNavigationActivity*>(param);
  self->displayTaskLoop();
}

void EpubReaderNavigationActivity::buildFilteredChapterList() {
  filteredSpineIndices.clear();

  for (int i = 0; i < epub->getSpineItemsCount(); i++) {
    // Skip footnote pages
    if (epub->shouldHideFromToc(i)) {
      Serial.printf("[%lu] [NAV] Hiding footnote page at spine index: %d\n", millis(), i);
      continue;
    }

    // Skip pages without TOC entry (unnamed pages)
    int tocIndex = epub->getTocIndexForSpineIndex(i);
    if (tocIndex == -1) {
      Serial.printf("[%lu] [NAV] Hiding unnamed page at spine index: %d\n", millis(), i);
      continue;
    }

    filteredSpineIndices.push_back(i);
  }

  Serial.printf("[%lu] [NAV] Filtered chapters: %d out of %d\n", millis(), filteredSpineIndices.size(),
                epub->getSpineItemsCount());
}

void EpubReaderNavigationActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  renderingMutex = xSemaphoreCreateMutex();

  // Chapters setup
  buildFilteredChapterList();

  // Default to Chapters tab, finding the current spine index
  currentTab = NavTab::Chapters;
  selectorIndex = 0;
  for (size_t i = 0; i < filteredSpineIndices.size(); i++) {
    if (filteredSpineIndices[i] == currentSpineIndex) {
      selectorIndex = static_cast<int>(i);
      break;
    }
  }
  if (hasSyncOption()) {
    selectorIndex += 1;  // Offset for top sync option
  }

  // Trigger first update
  updateRequired = true;
  xTaskCreate(&EpubReaderNavigationActivity::taskTrampoline, "EpubReaderNavTask", 4096,
              this,               // Parameters
              1,                  // Priority
              &displayTaskHandle  // Task handle
  );
}

void EpubReaderNavigationActivity::onExit() {
  ActivityWithSubactivity::onExit();

  // Wait until not rendering to delete task to avoid killing mid-instruction to EPD
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void EpubReaderNavigationActivity::launchSyncActivity() {
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  exitActivity();
  enterNewActivity(new KOReaderSyncActivity(
      renderer, mappedInput, epub, epubPath, currentSpineIndex, currentPage, totalPagesInSpine,
      [this]() {
        // On cancel
        exitActivity();
        updateRequired = true;
      },
      [this](int newSpineIndex, int newPage) {
        // On sync complete
        exitActivity();
        onSyncPosition(newSpineIndex, newPage);
      }));
  xSemaphoreGive(renderingMutex);
}

void EpubReaderNavigationActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  const bool upReleased = mappedInput.wasReleased(MappedInputManager::Button::Up);
  const bool downReleased = mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool leftReleased = mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool rightReleased = mappedInput.wasReleased(MappedInputManager::Button::Right);

  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  // Tab switching: Left/Right
  if (leftReleased && currentTab == NavTab::Footnotes) {
    currentTab = NavTab::Chapters;
    selectorIndex = 0;
    updateRequired = true;
    return;
  }
  if (rightReleased && currentTab == NavTab::Chapters) {
    currentTab = NavTab::Footnotes;
    selectorIndex = 0;
    updateRequired = true;
    return;
  }

  // Confirm button
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (currentTab == NavTab::Chapters) {
      if (hasSyncOption() && (selectorIndex == 0 || selectorIndex == totalItems - 1)) {
        launchSyncActivity();
      } else {
        int filteredIndex = selectorIndex;
        if (hasSyncOption()) filteredIndex -= 1;
        if (filteredIndex >= 0 && filteredIndex < static_cast<int>(filteredSpineIndices.size())) {
          onSelectSpineIndex(filteredSpineIndices[filteredIndex]);
        }
      }
    } else {
      // Footnotes tab
      const FootnoteEntry* entry = footnotes.getEntry(selectorIndex);
      if (entry) {
        onSelectFootnote(entry->href);
      }
    }
    return;
  }

  // Back button
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
    return;
  }

  // Navigation: Up/Down
  if (upReleased && totalItems > 0) {
    if (skipPage) {
      selectorIndex = ((selectorIndex / pageItems - 1) * pageItems + totalItems) % totalItems;
    } else {
      selectorIndex = (selectorIndex + totalItems - 1) % totalItems;
    }
    updateRequired = true;
  } else if (downReleased && totalItems > 0) {
    if (skipPage) {
      selectorIndex = ((selectorIndex / pageItems + 1) * pageItems) % totalItems;
    } else {
      selectorIndex = (selectorIndex + 1) % totalItems;
    }
    updateRequired = true;
  }
}

void EpubReaderNavigationActivity::displayTaskLoop() {
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

void EpubReaderNavigationActivity::renderScreen() {
  renderer.clearScreen();

  // Draw tab bar
  std::vector<TabInfo> tabs = {{"Chapters", currentTab == NavTab::Chapters},
                               {"Footnotes", currentTab == NavTab::Footnotes}};
  ScreenComponents::drawTabBar(renderer, TAB_BAR_Y, tabs);

  if (currentTab == NavTab::Chapters) {
    renderChaptersTab();
  } else {
    renderFootnotesTab();
  }

  // Draw scroll indicator
  const int screenHeight = renderer.getScreenHeight();
  const int contentHeight = screenHeight - CONTENT_START_Y - 60;
  ScreenComponents::drawScrollIndicator(renderer, getCurrentPage(), getTotalPages(), CONTENT_START_Y, contentHeight);

  // Draw side button hints (up/down navigation on right side)
  renderer.drawSideButtonHints(UI_10_FONT_ID, ">", "<");

  // Draw bottom button hints
  const auto labels = mappedInput.mapLabels("« Back", "Select", "<", ">");
  renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}

void EpubReaderNavigationActivity::renderChaptersTab() const {
  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  if (totalItems == 0) {
    renderer.drawText(UI_10_FONT_ID, LEFT_MARGIN, CONTENT_START_Y, "No chapters found");
    return;
  }

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;

  // Draw selection highlight
  renderer.fillRect(0, CONTENT_START_Y + (selectorIndex % pageItems) * LINE_HEIGHT - 2, pageWidth - RIGHT_MARGIN,
                    LINE_HEIGHT);

  for (int i = 0; i < pageItems; i++) {
    int itemIndex = pageStartIndex + i;
    if (itemIndex >= totalItems) break;

    const int displayY = CONTENT_START_Y + i * LINE_HEIGHT;
    const bool isSelected = (itemIndex == selectorIndex);

    if (isSyncItem(itemIndex)) {
      renderer.drawText(UI_10_FONT_ID, LEFT_MARGIN, displayY, ">> Sync Progress", !isSelected);
    } else {
      const int tocIndex = tocIndexFromItemIndex(itemIndex);
      if (tocIndex == -1) {
        renderer.drawText(UI_10_FONT_ID, LEFT_MARGIN, displayY, "Unnamed", !isSelected);
      } else {
        auto item = epub->getTocItem(tocIndex);
        const int indentSize = LEFT_MARGIN + (item.level - 1) * 15;
        const std::string chapterName =
            renderer.truncatedText(UI_10_FONT_ID, item.title.c_str(), pageWidth - RIGHT_MARGIN - indentSize);
        renderer.drawText(UI_10_FONT_ID, indentSize, displayY, chapterName.c_str(), !isSelected);
      }
    }
  }
}

void EpubReaderNavigationActivity::renderFootnotesTab() const {
  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems();
  const int totalItems = footnotes.getCount();

  if (totalItems == 0) {
    renderer.drawText(UI_10_FONT_ID, LEFT_MARGIN, CONTENT_START_Y, "No footnotes on this page");
    return;
  }

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;

  // Draw selection highlight
  renderer.fillRect(0, CONTENT_START_Y + (selectorIndex % pageItems) * LINE_HEIGHT - 2, pageWidth - RIGHT_MARGIN,
                    LINE_HEIGHT);

  for (int i = 0; i < pageItems; i++) {
    int itemIndex = pageStartIndex + i;
    if (itemIndex >= totalItems) break;

    const int displayY = CONTENT_START_Y + i * LINE_HEIGHT;
    const bool isSelected = (itemIndex == selectorIndex);
    const FootnoteEntry* entry = footnotes.getEntry(itemIndex);

    if (entry) {
      renderer.drawText(UI_10_FONT_ID, LEFT_MARGIN, displayY, entry->number, !isSelected);
    }
  }
}
