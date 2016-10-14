#ifndef __INC_XmlStrBuilder_H
#define __INC_XmlStrBuilder_H

/*
 * Copyright 2008-2016 Douglas Patriarche
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <sstream>
#include <deque>
#include <string>

// This class provides a convenient way of bullding an XML document and then converting it to a string.
class XmlStrBuilder_t
{
private:

    std::ostringstream os_m;

    typedef std::deque<std::string> Tags_t;
    Tags_t tags_m;

    int indent_m;

    char buf_am [160];

public:

    // Constructor
    XmlStrBuilder_t();

    // Clears the XML document.
    void clear();

    // Pushes a new tag onto the XML document stack.
    void pushTag(char const * tag);

    // Pops back a level on the XML document stack. The tag still remains, but new content added to the XML document will be at one level above.
    void popTag();

    // Adds a closed tag to the XML document, e.g. <tagName>content</tagName>
    void addTagAndValue(char const * tag, std::string const & str);

    // Adds a closed tag to the XML document where the content is in vararg format, e.g. <tagName>content</tagName>
    void addTagAndVararg(char const * tag, char const * format, ...);

    // Returns the entire XML document as a string.
    std::string str();

private:

    // Adds indentation to the current line.
    void indent();
};

#endif // __INC_XmlStrBuilder_H
