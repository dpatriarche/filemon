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

#include <stdarg.h>
#include <stdio.h>

#include "XmlStrBuilder.h"

//-----------------------------------------------------------------------------

XmlStrBuilder_t::XmlStrBuilder_t()
    : indent_m(0)
{}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::clear()
{
    os_m.str("");
    tags_m.clear();
    indent_m = 0;
}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::pushTag(char const * tag)
{
    std::string s(tag);
    tags_m.push_back(s);

    indent();
    os_m << "<" << tag << ">" << std::endl;

    indent_m += 1;
}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::popTag()
{
    if (!tags_m.empty()) {
        indent_m -= 1;
        indent();
        os_m << "</" << tags_m.back() << ">" <<std::endl;
        tags_m.pop_back();
    }
}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::addTagAndValue(char const * tag, std::string const & str)
{
    indent();
    os_m << "<" << tag << ">"
         << str
         << "</" << tag << ">"
         << std::endl;
}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::addTagAndVararg(char const * tag, char const * format, ...)
{
    va_list vargs;
    va_start(vargs, format);

    vsnprintf(buf_am, sizeof(buf_am), format, vargs);

    va_end(vargs);

    indent();
    os_m << "<" << tag << ">"
         << buf_am
         << "</" << tag << ">"
         << std::endl;
}

//-----------------------------------------------------------------------------

std::string XmlStrBuilder_t::str()
{
    return os_m.str();
}

//-----------------------------------------------------------------------------

void XmlStrBuilder_t::indent()
{
    for (int i = 0; i < indent_m; ++i) {
        os_m << "  ";
    }
}
