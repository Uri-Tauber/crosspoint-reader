#pragma once

struct FootnoteEntry {
  char number[8];
  char href[128];
  bool isInline;

  FootnoteEntry() : isInline(false) {
    number[0] = '\0';
    href[0] = '\0';
  }
};
