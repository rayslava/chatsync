#pragma once
#include <functional>
#include <locale>

namespace strutil {
  static inline void ltrim(std::string& str)
  {
    auto it = std::find_if( str.begin(), str.end(), [](char ch){
      return !std::isspace<char>(ch, std::locale::classic() );
    } );
    str.erase( str.begin(), it);
  }

  static inline void rtrim(std::string& str)
  {
    auto it = std::find_if( str.rbegin(), str.rend(), [](char ch){
      return !std::isspace<char>(ch, std::locale::classic() );
    } );
    str.erase( it.base(), str.end() );
  }

  static inline void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
  }

  static inline std::string trimmed(std::string s) {
    trim(s);
    return s;
  }

  /**
   * Predicate for case insensetive comparison
   */
  static inline bool cipred(unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
  }

  /**
   * Case insensetive comparison function
   */
  static inline bool cistrcmp(const std::string& a, const std::string& b) {
    return std::lexicographical_compare(a.begin(), a.end(),
                                        b.begin(), b.end(), cipred);
  }

}
