#pragma once
#include <functional>
#include "../Activity.h"

class EpubReaderMenuActivity final : public Activity {
 public:
  enum MenuOption { CHAPTERS, FOOTNOTES };

 private:
  int selectorIndex = 0;
  const std::function<void()> onGoBack;
  const std::function<void(MenuOption option)> onSelectOption;

  void renderScreen();

 public:
  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& inputManager,
                                  const std::function<void()>& onGoBack,
                                  const std::function<void(MenuOption option)>& onSelectOption)
      : Activity("EpubReaderMenuActivity", renderer, inputManager), onGoBack(onGoBack), onSelectOption(onSelectOption) {}

  void onEnter() override;
  void loop() override;
};
