#!/usr/bin/env python3
"""
Generate I18n C++ files from translations.csv

This script reads translations.csv and generates:
- I18nKeys.h: Language enum, LANGUAGE_NAMES array, StrId enum
- I18nStrings.h: String array declarations
- I18nStrings.cpp: String array definitions with all translations

Usage:
    python gen_i18n.py <path_to_translations.csv> <output_directory>
    
Example:
    python gen_i18n.py lib/I18n/translations.csv lib/I18n/
"""

import csv
import sys
import os
import re
from pathlib import Path
from typing import List, Dict, Tuple


def escape_cpp_string(s: str) -> str:
    r"""
    Convert a string to a proper C++ string literal.
    Handles:
    - Existing \xNN escape sequences
    - Quote escaping
    - Newline handling
    - Forces non-ASCII characters to \xNN hex sequences (UTF-8 bytes)
    - Breaks string literals after hex sequences to prevent "out of range" errors
    """
    if not s:
        return '""'
    
    s = s.replace('\n', '\\n')
    
    # Build the escaped string
    result = []
    i = 0
    while i < len(s):
        char = s[i]
        
        # Check for existing backslash escape sequences
        if char == '\\' and i + 1 < len(s):
            next_char = s[i+1]
            if next_char in ['n', 't', 'r', '"', '\\']:
                result.append(char)
                result.append(next_char)
                i += 2
            elif next_char == 'x' and i + 3 < len(s):
                # Valid existing hex escape
                result.append(s[i:i+4])
                # Add string break "" to prevent hex overflow
                result.append('""') 
                i += 4
            else:
                result.append('\\\\')
                i += 1
        # Escape quotes
        elif char == '"':
            result.append('\\"')
            i += 1
        else:
            # Check if character is ASCII (0-127)
            if ord(char) < 128:
                result.append(char)
                i += 1
            else:
                # Non-ASCII: Encode to UTF-8 bytes
                utf8_bytes = char.encode('utf-8')
                for b in utf8_bytes:
                    # Append hex code AND an empty string break ""
                    # Example: "Fran\xC3""\xA7""ais"
                    result.append(f'\\x{b:02X}""')
                i += 1
    
    return '"' + ''.join(result) + '"'

def compute_character_set(translations: Dict[str, List[str]], lang_index: int) -> str:
    """
    Compute the sorted set of unique Unicode characters used in a language's translations.
    
    Args:
        translations: Dictionary mapping keys to list of translations
        lang_index: Index of the language to compute character set for
    
    Returns:
        UTF-8 encoded string containing all unique characters, sorted by codepoint
    """
    unique_chars = set()
    
    for key, trans_list in translations.items():
        text = trans_list[lang_index]
        if not text:
            continue
        
        for char in text:
            unique_chars.add(ord(char))
    
    sorted_chars = sorted(unique_chars)
    charset = ''.join(chr(cp) for cp in sorted_chars)
    
    return charset


def is_valid_identifier(name: str) -> bool:
    """Check if a string is a valid C++ identifier."""
    if not name:
        return False
    return bool(re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', name))


def get_lang_abbreviation(lang_code: str, lang_name: str) -> str:
    """Convert language to 2-letter abbreviation.
    
    Tries to derive ISO 639-1 code from the native language name.
    Falls back to first 2 chars of language code if needed.
    
    Args:
        lang_code: Language code from CSV header (e.g., "ENGLISH", "SPANISH")
        lang_name: Native language name from row 2 (e.g., "English", "Español")
    
    Returns:
        2-letter abbreviation (e.g., "EN", "ES")
    """
    # Map of language names (lowercase) to ISO 639-1 codes
    name_to_code = {
        'english': 'EN',
        'español': 'ES',
        'espanol': 'ES',
        'italiano': 'IT',
        'svenska': 'SV',
        'français': 'FR',
        'francais': 'FR',
        'deutsch': 'DE',
        'german': 'DE',
        'português': 'PT',
        'portugues': 'PT',
        '中文': 'ZH',
        'chinese': 'ZH',
        '日本語': 'JA',
        'japanese': 'JA',
        '한국어': 'KO',
        'korean': 'KO',
        'русский': 'RU',
        'russian': 'RU',
        'العربية': 'AR',
        'arabic': 'AR',
        'עברית': 'HE',
        'hebrew': 'HE',
        'فارسی': 'FA',
        'persian': 'FA',
    }
    
    # Try to map from language name first
    lang_name_lower = lang_name.lower()
    if lang_name_lower in name_to_code:
        return name_to_code[lang_name_lower]
    
    # Fallback: use first 2 chars of language code
    return lang_code[:2].upper()


def is_valid_identifier(name: str) -> bool:
    """Check if a string is a valid C++ identifier."""
    if not name:
        return False
    return bool(re.match(r'^[a-zA-Z_][a-zA-Z0-9_]*$', name))


def read_csv(csv_path: str) -> Tuple[List[str], List[str], Dict[str, List[str]]]:
    """
    Read translations CSV and extract:
    - languages: List of language codes from header
    - language_names: List of language display names
    - translations: Dict mapping string key to list of translations
    
    Returns: (languages, language_names, translations)
    """
    # Try different encodings
    encodings_to_try = [
        'utf-8',           # Universal, handles all languages including RTL
        'utf-8-sig',       # UTF-8 with BOM (common from Excel saves)
        'latin-1',         # Western European
        'iso-8859-1',      # Western European (alias)
        'cp1252',          # Windows Western European
        'iso-8859-8',      # Hebrew
        'cp1255',          # Windows Hebrew
        'iso-8859-6',      # Arabic
        'cp1256',          # Windows Arabic
    ]
    
    detected_encoding = None
    for encoding in encodings_to_try:
        try:
            with open(csv_path, 'r', encoding=encoding) as f:
                content = f.read()
                # Verify we can actually parse it as CSV
                import io
                csv.reader(io.StringIO(content))
            detected_encoding = encoding
            print(f"Successfully read CSV with {encoding} encoding")
            break
        except (UnicodeDecodeError, csv.Error):
            continue
    
    if not detected_encoding:
        raise ValueError(f"Could not decode CSV file with any supported encoding. Tried: {', '.join(encodings_to_try)}")
    
    # Now parse with detected encoding
    with open(csv_path, 'r', encoding=detected_encoding) as f:
        reader = csv.reader(f)
        
        # Read header row
        header = next(reader)
        if header[0] != 'KEYS':
            raise ValueError(f"Expected 'KEYS' in first column, got '{header[0]}'")
        
        languages = [lang.upper() for lang in header[1:]]
        if not languages:
            raise ValueError("No languages found in CSV header")
        
        print(f"Found languages: {', '.join(languages)}")
        
        # Read language names row (row 2)
        lang_names_row = next(reader)
        if lang_names_row[0] != '':
            raise ValueError(f"Expected empty key in row 2, got '{lang_names_row[0]}'")
        
        language_names = lang_names_row[1:len(languages)+1]
        if len(language_names) != len(languages):
            raise ValueError(f"Language names count ({len(language_names)}) doesn't match languages count ({len(languages)})")
        
        # Validate language names
        if not all(name.strip() for name in language_names):
            raise ValueError("All language names must be non-empty")
        
        # Check for duplicate language names
        if len(language_names) != len(set(language_names)):
            duplicates = [name for name in language_names if language_names.count(name) > 1]
            raise ValueError(f"Duplicate language names found: {', '.join(set(duplicates))}")
        
        # Check if row 2 has extra columns
        if len(lang_names_row) > len(languages) + 1:
            extra_cols = len(lang_names_row) - len(languages) - 1
            print(f"WARNING: Row 2 has {extra_cols} extra column(s) beyond defined languages - these will be ignored")
        
        print(f"Language names: {', '.join(language_names)}")
        
        # Read all translation rows
        translations = {}
        row_num = 3  # Starting from row 3 (after header and language names)
        missing_translations = []
        
        for row in reader:
            if not row or not row[0]:  # Skip empty rows
                row_num += 1
                continue
            
            key = row[0].strip()
            if not key:
                row_num += 1
                continue
            
            # Validate key is a valid C++ identifier
            if not is_valid_identifier(key):
                raise ValueError(f"Invalid identifier at row {row_num}: '{key}'")
            
            # Check for duplicates
            if key in translations:
                raise ValueError(f"Duplicate key at row {row_num}: '{key}'")
            
            # Get translations for this key
            trans = row[1:len(languages)+1]
            if len(trans) < len(languages):
                # Pad with empty strings if row is short
                trans.extend([''] * (len(languages) - len(trans)))
            
            # Warn if row has extra columns beyond defined languages
            if len(row) > len(languages) + 1:
                extra_cols = len(row) - len(languages) - 1
                print(f"WARNING: Row {row_num} (key '{key}') has {extra_cols} extra column(s) - these will be ignored")
            
            # Check for missing translations
            for i, t in enumerate(trans):
                if not t.strip():
                    missing_translations.append(f"Row {row_num}, {key}, {languages[i]}")
            
            translations[key] = trans
            row_num += 1
        
        print(f"Loaded {len(translations)} translation keys")
        
        if missing_translations:
            print(f"\nWARNING: Found {len(missing_translations)} missing translations:")
            for msg in missing_translations[:10]:  # Show first 10
                print(f"  - {msg}")
            if len(missing_translations) > 10:
                print(f"  ... and {len(missing_translations) - 10} more")
        
        return languages, language_names, translations


def generate_keys_header(languages: List[str], language_names: List[str], 
                         string_keys: List[str], output_path: str):
    """Generate I18nKeys.h file."""
    
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        "// THIS FILE IS AUTO-GENERATED BY gen_i18n.py. DO NOT EDIT.",
        "",
        "// Forward declaration for string arrays",
        "namespace i18n_strings {",
    ]
    
    # Forward declare all string arrays
    for i, lang in enumerate(languages):
        abbrev = get_lang_abbreviation(lang, language_names[i])
        lines.append(f"  extern const char* const STRINGS_{abbrev}[];")
    
    lines.append("}")
    lines.append("")
    
    # Generate Language enum
    lines.append("// Language enum")
    lines.append("enum class Language : uint8_t {")
    for i, lang in enumerate(languages):
        lines.append(f"  {lang} = {i},")
    lines.append("  _COUNT")
    lines.append("};")
    lines.append("")
    
    # Generate LANGUAGE_NAMES extern declaration (definition will be in .cpp)
    lines.append("// Language display names (defined in I18nStrings.cpp)")
    lines.append("extern const char* const LANGUAGE_NAMES[];")
    lines.append("")
    
    # Generate CHARACTER_SETS extern declaration (definition will be in .cpp)
    lines.append("// Character sets for each language (defined in I18nStrings.cpp)")
    lines.append("extern const char* const CHARACTER_SETS[];")
    lines.append("")
    
    # Generate StrId enum (forward declaration needed for getStringArray)
    lines.append("// String IDs")
    lines.append("enum class StrId : uint16_t {")
    for key in string_keys:
        lines.append(f"  {key},")
    lines.append("  // Sentinel - must be last")
    lines.append("  _COUNT")
    lines.append("};")
    lines.append("")
    
    # Generate helper function to get string array by language
    lines.append("// Helper function to get string array for a language")
    lines.append("inline const char* const* getStringArray(Language lang) {")
    lines.append("  switch (lang) {")
    for i, lang in enumerate(languages):
        abbrev = get_lang_abbreviation(lang, language_names[i])
        lines.append(f"    case Language::{lang}:")
        lines.append(f"      return i18n_strings::STRINGS_{abbrev};")
    lines.append("    default:")
    # Default to first language (typically English)
    first_abbrev = get_lang_abbreviation(languages[0], language_names[0])
    lines.append(f"      return i18n_strings::STRINGS_{first_abbrev};")
    lines.append("  }")
    lines.append("}")
    lines.append("")
    
    # Add helper function for language count
    lines.append("// Helper function to get language count")
    lines.append("constexpr uint8_t getLanguageCount() {")
    lines.append("  return static_cast<uint8_t>(Language::_COUNT);")
    lines.append("}")
    
    # Write file
    with open(output_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines))
        f.write('\n')
    
    print(f"Generated: {output_path}")


def generate_strings_header(languages: List[str], language_names: List[str], output_path: str):
    """Generate I18nStrings.h file."""
    
    lines = [
        "#pragma once",
        "#include <string>",
        '#include "I18nKeys.h"',
        "",
        "// THIS FILE IS AUTO-GENERATED BY gen_i18n.py. DO NOT EDIT.",
        "",
        "namespace i18n_strings {",
        "",
    ]
    
    # Declare arrays for each language
    for i, lang in enumerate(languages):
        abbrev = get_lang_abbreviation(lang, language_names[i])
        lines.append(f"extern const char* const STRINGS_{abbrev}[];")
    
    lines.append("")
    lines.append("}  // namespace i18n_strings")
    
    # Write file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
        f.write('\n')
    
    print(f"Generated: {output_path}")


def generate_strings_cpp(languages: List[str], language_names: List[str],
                         string_keys: List[str], translations: Dict[str, List[str]], 
                         output_path: str):
    """Generate I18nStrings.cpp file."""
    
    lines = [
        '#include "I18nStrings.h"',
        "",
        "// THIS FILE IS AUTO-GENERATED BY gen_i18n.py. DO NOT EDIT.",
        "",
    ]
    
    # Generate LANGUAGE_NAMES array definition (declared extern in I18nKeys.h)
    lines.append("// Language display names")
    lines.append("const char* const LANGUAGE_NAMES[] = {")
    for name in language_names:
        lines.append(f"  {escape_cpp_string(name)},")
    lines.append("};")
    lines.append("")
    
    # Compute and generate CHARACTER_SETS array
    lines.append("// Character sets for each language")
    lines.append("const char* const CHARACTER_SETS[] = {")
    for lang_idx in range(len(languages)):
        charset = compute_character_set(translations, lang_idx)
        lines.append(f"  {escape_cpp_string(charset)},  // {language_names[lang_idx]}")
    lines.append("};")
    lines.append("")
    # -------------------------------

    lines.append("namespace i18n_strings {")
    lines.append("")
    
    # Generate array for each language
    for lang_idx, lang in enumerate(languages):
        abbrev = get_lang_abbreviation(lang, language_names[lang_idx])
        lines.append(f"const char* const STRINGS_{abbrev}[] = {{")
        
        for key in string_keys:
            trans = translations[key][lang_idx]
            # Use English as fallback if translation is missing
            if not trans.strip():
                trans = translations[key][0]  # Fallback to English (first language)
            lines.append(f"  {escape_cpp_string(trans)},")
        
        lines.append("};")
        lines.append("")
    
    lines.append("}  // namespace i18n_strings")
    lines.append("")
    
    # Generate compile-time checks
    lines.append("// Compile-time validation of array sizes")
    for lang_idx, lang in enumerate(languages):
        abbrev = get_lang_abbreviation(lang, language_names[lang_idx])
        lines.append(f"static_assert(sizeof(i18n_strings::STRINGS_{abbrev}) / sizeof(i18n_strings::STRINGS_{abbrev}[0]) ==")
        lines.append(f"                  static_cast<size_t>(StrId::_COUNT),")
        lines.append(f'              "STRINGS_{abbrev} size mismatch");')
    
    # Write file
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
        f.write('\n')
    
    print(f"Generated: {output_path}")

def main():
    if len(sys.argv) != 3:
        print("Usage: python gen_i18n.py <translations.csv> <output_directory>")
        print("Example: python gen_i18n.py lib/I18n/translations.csv lib/I18n/")
        sys.exit(1)
    
    csv_path = sys.argv[1]
    output_dir = sys.argv[2]
    
    # Validate inputs
    if not os.path.exists(csv_path):
        print(f"Error: CSV file not found: {csv_path}")
        sys.exit(1)
    
    if not os.path.isdir(output_dir):
        print(f"Error: Output directory not found: {output_dir}")
        sys.exit(1)
    
    print(f"Reading translations from: {csv_path}")
    print(f"Output directory: {output_dir}")
    print()
    
    try:
        languages, language_names, translations = read_csv(csv_path)
        
        # Get ordered list of keys
        string_keys = list(translations.keys())
        
        # Generate output files
        output_dir = Path(output_dir)
        
        generate_keys_header(
            languages, 
            language_names,
            string_keys,
            str(output_dir / "I18nKeys.h")
        )
        
        generate_strings_header(
            languages,
            language_names,
            str(output_dir / "I18nStrings.h")
        )
        
        generate_strings_cpp(
            languages,
            language_names,
            string_keys,
            translations,
            str(output_dir / "I18nStrings.cpp")
        )
        
        print()
        print("✓ Code generation complete!")
        print(f"  Languages: {len(languages)}")
        print(f"  String keys: {len(string_keys)}")
        
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()