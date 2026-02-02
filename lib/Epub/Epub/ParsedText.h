#pragma once

#include <EpdFontFamily.h>

#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "FootnoteEntry.h"
#include "blocks/TextBlock.h"

class GfxRenderer;

class ParsedText {
  std::list<std::string> words;
  std::list<EpdFontFamily::Style> wordStyles;
  std::deque<uint8_t> wordHasFootnote;
  std::deque<FootnoteEntry> footnoteQueue;
  std::deque<std::vector<std::string>> wordAnchors;
  TextBlock::Style style;
  bool extraParagraphSpacing;
  bool hyphenationEnabled;

  void applyParagraphIndent();
  std::vector<size_t> computeLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth, int spaceWidth,
                                        std::vector<uint16_t>& wordWidths);
  std::vector<size_t> computeHyphenatedLineBreaks(const GfxRenderer& renderer, int fontId, int pageWidth,
                                                  int spaceWidth, std::vector<uint16_t>& wordWidths);
  bool hyphenateWordAtIndex(size_t wordIndex, int availableWidth, const GfxRenderer& renderer, int fontId,
                            std::vector<uint16_t>& wordWidths, bool allowFallbackBreaks);
  void extractLine(
      size_t breakIndex, int pageWidth, int spaceWidth, const std::vector<uint16_t>& wordWidths,
      const std::vector<size_t>& lineBreakIndices,
      const std::function<void(std::shared_ptr<TextBlock>, const std::vector<FootnoteEntry>&,
                               const std::vector<std::string>&)>& processLine);
  std::vector<uint16_t> calculateWordWidths(const GfxRenderer& renderer, int fontId);

 public:
  explicit ParsedText(const TextBlock::Style style, const bool extraParagraphSpacing,
                      const bool hyphenationEnabled = false)
      : style(style), extraParagraphSpacing(extraParagraphSpacing), hyphenationEnabled(hyphenationEnabled) {}
  ~ParsedText() = default;

  void addWord(std::string word, EpdFontFamily::Style fontStyle, std::unique_ptr<FootnoteEntry> footnote = nullptr,
               std::vector<std::string> anchors = {});
  void setStyle(const TextBlock::Style style) { this->style = style; }
  TextBlock::Style getStyle() const { return style; }
  size_t size() const { return words.size(); }
  bool isEmpty() const { return words.empty(); }
  void layoutAndExtractLines(
      const GfxRenderer& renderer, int fontId, uint16_t viewportWidth,
      const std::function<void(std::shared_ptr<TextBlock>, const std::vector<FootnoteEntry>&,
                               const std::vector<std::string>&)>& processLine,
      bool includeLastLine = true);
};
