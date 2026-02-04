#include "EpubReaderChapterSelectionActivity.h"

#include <GfxRenderer.h>

#include "KOReaderCredentialStore.h"
#include "KOReaderSyncActivity.h"
#include "MappedInputManager.h"
#include "ScreenComponents.h"
#include "fontIds.h"

namespace {
constexpr int SKIP_PAGE_MS = 700;
constexpr int CHAPTER_LINE_HEIGHT = 30;
constexpr int CONTENT_START_Y = 60;
}  // namespace

bool EpubReaderChapterSelectionActivity::hasSyncOption() const { return KOREADER_STORE.hasCredentials(); }

int EpubReaderChapterSelectionActivity::getTotalItems() const {
  const int syncCount = hasSyncOption() ? 2 : 0;
  return epub->getTocItemsCount() + syncCount;
}

bool EpubReaderChapterSelectionActivity::isSyncItem(int index) const {
  if (!hasSyncOption()) return false;
  return index == 0 || index == getTotalItems() - 1;
}

int EpubReaderChapterSelectionActivity::tocIndexFromItemIndex(int itemIndex) const {
  const int offset = hasSyncOption() ? 1 : 0;
  return itemIndex - offset;
}

int EpubReaderChapterSelectionActivity::getPageItems() const {
  const int screenHeight = renderer.getScreenHeight();
  const int availableHeight = screenHeight - CONTENT_START_Y - 60;
  int items = availableHeight / CHAPTER_LINE_HEIGHT;
  return (items < 1) ? 1 : items;
}

int EpubReaderChapterSelectionActivity::getCurrentSelectionPage() const {
  const int pageItems = getPageItems();
  return selectorIndex / (pageItems > 0 ? pageItems : 1) + 1;
}

int EpubReaderChapterSelectionActivity::getTotalSelectionPages() const {
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();
  if (totalItems == 0) return 1;
  return (totalItems + pageItems - 1) / (pageItems > 0 ? pageItems : 1);
}

void EpubReaderChapterSelectionActivity::taskTrampoline(void* param) {
  auto* self = static_cast<EpubReaderChapterSelectionActivity*>(param);
  self->displayTaskLoop();
}

void EpubReaderChapterSelectionActivity::onEnter() {
  ActivityWithSubactivity::onEnter();

  if (!epub) {
    return;
  }

  renderingMutex = xSemaphoreCreateMutex();

  const int syncOffset = hasSyncOption() ? 1 : 0;
  selectorIndex = epub->getTocIndexForSpineIndex(currentSpineIndex);
  if (selectorIndex == -1) {
    selectorIndex = 0;
  }
  selectorIndex += syncOffset;

  updateRequired = true;
  xTaskCreate(&EpubReaderChapterSelectionActivity::taskTrampoline, "EpubChapterSelectTask", 4096, this, 1,
              &displayTaskHandle);
}

void EpubReaderChapterSelectionActivity::onExit() {
  ActivityWithSubactivity::onExit();
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  if (displayTaskHandle) {
    vTaskDelete(displayTaskHandle);
    displayTaskHandle = nullptr;
  }
  vSemaphoreDelete(renderingMutex);
  renderingMutex = nullptr;
}

void EpubReaderChapterSelectionActivity::launchSyncActivity() {
  xSemaphoreTake(renderingMutex, portMAX_DELAY);
  exitActivity();
  enterNewActivity(new KOReaderSyncActivity(
      renderer, mappedInput, epub, epubPath, currentSpineIndex, currentPage, totalPagesInSpine,
      [this]() {
        exitActivity();
        updateRequired = true;
      },
      [this](int newSpineIndex, int newPage) {
        exitActivity();
        onSyncPosition(newSpineIndex, newPage);
      }));
  xSemaphoreGive(renderingMutex);
}

void EpubReaderChapterSelectionActivity::loop() {
  if (subActivity) {
    subActivity->loop();
    return;
  }

  const bool prevReleased = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                            mappedInput.wasReleased(MappedInputManager::Button::Left);
  const bool nextReleased = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                            mappedInput.wasReleased(MappedInputManager::Button::Right);

  const bool skipPage = mappedInput.getHeldTime() > SKIP_PAGE_MS;
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (isSyncItem(selectorIndex)) {
      launchSyncActivity();
      return;
    }

    const int tocIndex = tocIndexFromItemIndex(selectorIndex);
    const auto newSpineIndex = epub->getSpineIndexForTocIndex(tocIndex);
    if (newSpineIndex == -1) {
      onGoBack();
    } else {
      onSelectSpineIndex(newSpineIndex);
    }
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoBack();
  } else if (prevReleased) {
    if (totalItems > 0) {
      if (skipPage) {
        selectorIndex = ((selectorIndex / pageItems - 1) * pageItems + totalItems) % totalItems;
      } else {
        selectorIndex = (selectorIndex + totalItems - 1) % totalItems;
      }
      updateRequired = true;
    }
  } else if (nextReleased) {
    if (totalItems > 0) {
      if (skipPage) {
        selectorIndex = ((selectorIndex / pageItems + 1) * pageItems) % totalItems;
      } else {
        selectorIndex = (selectorIndex + 1) % totalItems;
      }
      updateRequired = true;
    }
  }
}

void EpubReaderChapterSelectionActivity::displayTaskLoop() {
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

void EpubReaderChapterSelectionActivity::renderScreen() {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const int pageItems = getPageItems();
  const int totalItems = getTotalItems();

  renderer.drawCenteredText(UI_12_FONT_ID, 15, "Go to Chapter", true, EpdFontFamily::BOLD);

  const auto pageStartIndex = selectorIndex / pageItems * pageItems;
  renderer.fillRect(0, CONTENT_START_Y + (selectorIndex % pageItems) * CHAPTER_LINE_HEIGHT - 2, pageWidth - 1,
                    CHAPTER_LINE_HEIGHT);

  for (int i = 0; i < pageItems; i++) {
    int itemIndex = pageStartIndex + i;
    if (itemIndex >= totalItems) break;
    const int displayY = CONTENT_START_Y + i * CHAPTER_LINE_HEIGHT;
    const bool isSelected = (itemIndex == selectorIndex);

    if (isSyncItem(itemIndex)) {
      renderer.drawText(UI_10_FONT_ID, 20, displayY, ">> Sync Progress", !isSelected);
    } else {
      const int tocIndex = tocIndexFromItemIndex(itemIndex);
      auto item = epub->getTocItem(tocIndex);

      const int indentSize = 20 + (item.level > 0 ? item.level - 1 : 0) * 15;
      const char* titleStr = item.title.empty() ? "Unnamed" : item.title.c_str();
      const std::string chapterName = renderer.truncatedText(UI_10_FONT_ID, titleStr, pageWidth - 40 - indentSize);

      renderer.drawText(UI_10_FONT_ID, indentSize, displayY, chapterName.c_str(), !isSelected);
    }
  }

  const int availableHeight = renderer.getScreenHeight() - CONTENT_START_Y - 60;
  ScreenComponents::drawScrollIndicator(renderer, getCurrentSelectionPage(), getTotalSelectionPages(), CONTENT_START_Y,
                                        availableHeight);

  if (renderer.getOrientation() != GfxRenderer::LandscapeClockwise) {
    const auto labels = mappedInput.mapLabels("« Back", "Select", "Up", "Down");
    renderer.drawButtonHints(UI_10_FONT_ID, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();
}
