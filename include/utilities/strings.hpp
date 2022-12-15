#ifndef UTILITIES_STRING_HPP
#define UTILITIES_STRING_HPP

#include <string>
#include <vector>

// String helpers
bool startsWith(std::string_view str, std::string_view prefix);
bool endsWith(std::string_view str, std::string_view suffix);
std::vector<std::string> split(std::string s, std::string delimiter);

#endif // UTILITIES_STRING_HPP