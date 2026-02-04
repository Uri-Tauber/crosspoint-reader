#pragma once
#include <cstring>
#include <functional>
#include <memory>

#include "../../lib/Epub/Epub/FootnoteEntry.h"
#include "../Activity.h"
#include "FootnotesData.h"

class EpubReaderFootnotesActivity final : public Activity {
  const FootnotesData& footnotes;
  const std::function<void()> onGoBack;
  const std::function<void(const char*)> onSelectFootnote;
  int selectedIndex;

 public:
  EpubReaderFootnotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const FootnotesData& footnotes,
                              const std::function<void()>& onGoBack,
                              const std::function<void(const char*)>& onSelectFootnote)
      : Activity("EpubReaderFootnotes", renderer, mappedInput),
        footnotes(footnotes),
        onGoBack(onGoBack),
        onSelectFootnote(onSelectFootnote),
        selectedIndex(0) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;

 private:
  void render();
};