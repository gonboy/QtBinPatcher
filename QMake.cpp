/*******************************************************************************

         Yuri V. Krugloff. 2013-2014. http://www.tver-soft.org

    This is free and unencumbered software released into the public domain.

    Anyone is free to copy, modify, publish, use, compile, sell, or
    distribute this software, either in source code form or as a compiled
    binary, for any purpose, commercial or non-commercial, and by any
    means.

    In jurisdictions that recognize copyright laws, the author or authors
    of this software dedicate any and all copyright interest in the
    software to the public domain. We make this dedication for the benefit
    of the public at large and to the detriment of our heirs and
    successors. We intend this dedication to be an overt act of
    relinquishment in perpetuity of all present and future rights to this
    software under copyright law.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
    IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
    OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
    ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.

    For more information, please refer to <http://unlicense.org/>

*******************************************************************************/

#include "QMake.hpp"

#include <string.h>

#include "Functions.hpp"
#include "Logger.hpp"

//------------------------------------------------------------------------------

using namespace std;
using namespace Functions;

//------------------------------------------------------------------------------

const string TQMake::m_QMakeName(
          #ifdef OS_WINDOWS
              "qmake.exe"
          #else
              "qmake"
          #endif
      );
const string TQMake::m_BinDirName("bin");

//------------------------------------------------------------------------------

TQMake::TQMake()
    : m_QtVersion('\0')
{
}

//------------------------------------------------------------------------------

bool TQMake::find(const string& qtDir)
{
    m_QMakePath.clear();
    m_QtPath.clear();
    m_QtVersion = '\0';

    if (!qtDir.empty()) {
        m_QMakePath = qtDir + nativeSeparator() + m_BinDirName + nativeSeparator() + m_QMakeName;
        if (isFileExists(m_QMakePath))
            m_QtPath = qtDir;
        else
            m_QMakePath.clear();
    }
    else {
        string CurrentDir = currentDir();
        m_QMakePath = CurrentDir + nativeSeparator() + m_QMakeName;
        if (isFileExists(m_QMakePath)) {
            // One level up.
            string::size_type pos = CurrentDir.find_last_of(nativeSeparator());
            if (pos != string::npos && pos < CurrentDir.length() - 1)
                CurrentDir.resize(pos);
            m_QtPath = CurrentDir;
        }
        else {
            m_QMakePath = CurrentDir + nativeSeparator() + m_BinDirName + nativeSeparator() + m_QMakeName;
            if (!isFileExists(m_QMakePath))
                m_QMakePath.clear();
            else
                m_QtPath = CurrentDir;
        }
    }
    return !m_QMakePath.empty();
}

//------------------------------------------------------------------------------

bool TQMake::query()
{
    m_QMakeOutput.clear();
    if (!m_QMakePath.empty()) {
        string QMakeStart = "\"\"" + m_QMakePath + "\" -query\"";
        LOG_V("qmake command line: %s.\n", QMakeStart.c_str());
        m_QMakeOutput = getProgramOutput(QMakeStart);
        LOG_V("\nqmake output:\n%s\n", m_QMakeOutput.c_str());
    }
    return !m_QMakeOutput.empty();
}

//------------------------------------------------------------------------------

bool TQMake::parse()
{
    m_QMakeValues.clear();
    m_QtVersion = '\0';

    if (m_QMakeOutput.empty())
        return false;

    const char* const Delimiters = "\r\n";
    char* s = strtok(const_cast<char*>(m_QMakeOutput.c_str()), Delimiters);
    while (s != NULL) {
        string str = s;
        string::size_type i = str.find(':');
        if (i != string::npos) {
            m_QMakeValues[str.substr(0, i)] = str.substr(i + 1);
        }
        else {
            LOG_E("Error parsing qmake output string:\n"
                  "  \"%s\"", s);
            return false;
        }
        s = strtok(NULL, Delimiters);
    }

    TStringMap::const_iterator Iter = m_QMakeValues.find("QT_VERSION");
    if (Iter != m_QMakeValues.end()) {
        const string& Version = Iter->second;
        if (!Version.empty())
            m_QtVersion = Version[0];
    }

    LOG_V("\nParsed qmake variables:\n");
    for (TStringMap::const_iterator I = m_QMakeValues.begin(); I != m_QMakeValues.end(); ++I)
        LOG_V("  %s = \"%s\"\n", I->first.c_str(), I->second.c_str());

    return true;
}

//------------------------------------------------------------------------------

string TQMake::value(const string& variable) const
{
    TStringMap::const_iterator Iter = m_QMakeValues.find(variable);
    if (Iter != m_QMakeValues.end())
        return Iter->second;
    return string();
}

//------------------------------------------------------------------------------
