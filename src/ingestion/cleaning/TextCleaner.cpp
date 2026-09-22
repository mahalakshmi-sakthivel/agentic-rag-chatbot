#include "TextCleaner.h"

#include <cctype>
#include <sstream>

namespace ingestion::cleaning
{

std::string TextCleaner::clean(
    const std::string &text)
{
    std::stringstream input(text);
    std::string line;
    std::string output;

    while (std::getline(input, line))
    {
        std::string cleanedLine;

        for (char character : line)
        {
            if (!std::isspace(
                    static_cast<unsigned char>(character)))
            {
                cleanedLine += character;
            }
            else if (!cleanedLine.empty() &&
                     cleanedLine.back() != ' ')
            {
                cleanedLine += ' ';
            }
        }

        if (!cleanedLine.empty())
        {
            if (cleanedLine.back() == ' ')
            {
                cleanedLine.pop_back();
            }

            output += cleanedLine;
            output += '\n';
        }
    }

    return output;
}

} // namespace ingestion::cleaning
