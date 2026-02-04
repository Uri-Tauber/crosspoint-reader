#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <memory>
#include <string>

#include "../ActivityWithSubactivity.h"
#include "FootnotesData.h"

class EpubReaderFootnoteSelectionActivity final : public ActivityWithSubactivity {
  const FootnotesData& footnotes;
  TaskHandle_t displayTaskHandle = nullptr;
  SemaphoreHandle_t renderingMutex = nullptr;
  int selectedIndex = 0;
  bool updateRequired = false;

  const std::function<void()> onGoBack;
  const std::function<void(const char* href)> onSelectFootnote;

  static void taskTrampoline(void* param);
  [[noreturn]] void displayTaskLoop();
  void renderScreen();

 public:
  explicit EpubReaderFootnoteSelectionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                               const FootnotesData& footnotes, std::function<void()> onGoBack,
                                               std::function<void(const char*)> onSelectFootnote)
      : ActivityWithSubactivity("EpubReaderFootnoteSelection", renderer, mappedInput),
        footnotes(footnotes),
        onGoBack(onGoBack),
        onSelectFootnote(onSelectFootnote) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
};
