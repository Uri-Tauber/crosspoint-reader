#pragma once
#include <functional>
#include <vector>
#include "../Activity.h"
#include "../../lib/Epub/Epub/FootnoteEntry.h"

class EpubReaderFootnotesActivity final : public Activity {
 private:
  const std::vector<FootnoteEntry>& footnotes;
  int selectorIndex = 0;
  const std::function<void()> onGoBack;
  const std::function<void(const char*)> onSelectFootnote;

  void renderScreen();

 public:
  explicit EpubReaderFootnotesActivity(GfxRenderer& renderer, MappedInputManager& inputManager,
                                       const std::vector<FootnoteEntry>& footnotes,
                                       const std::function<void()>& onGoBack,
                                       const std::function<void(const char*)>& onSelectFootnote)
      : Activity("EpubReaderFootnotesActivity", renderer, inputManager),
        footnotes(footnotes),
        onGoBack(onGoBack),
        onSelectFootnote(onSelectFootnote) {}

  void onEnter() override;
  void loop() override;
};
