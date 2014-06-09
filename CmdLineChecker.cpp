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

#include "CmdLineChecker.hpp"

#include <stdio.h>

//------------------------------------------------------------------------------

using namespace std;

//------------------------------------------------------------------------------

TCmdLineChecker::TCmdLineChecker(const TStringListMap& argsMap)
    : m_ArgsMap(argsMap)
{
}

//------------------------------------------------------------------------------

void TCmdLineChecker::check(const string& option, const TCmdLineChecker::TOptionType optionType)
{
    TStringListMap::iterator Iter = m_ArgsMap.find(option);
    if (Iter != m_ArgsMap.end())
    {
        switch (optionType)
        {
            case otNoValue :
                if (!Iter->second.empty())
                    m_ErrorString += "Option --" + option + " cannot have value.\n";
                break;
            case otSingleValue :
                if (Iter->second.size() > 1)
                    m_ErrorString += "Option --" + option + " can be only one.\n";
                // break is missing! It's right!
            case otMultyValue :
                if (Iter->second.empty())
                    m_ErrorString += "Option --" + option + " must have value.\n";
                break;
        }
        m_ArgsMap.erase(Iter);
    }
}

//------------------------------------------------------------------------------

void TCmdLineChecker::checkIncompatible(const string& option1, const string& option2)
{
    if (m_ArgsMap.find(option1) != m_ArgsMap.end() && m_ArgsMap.find(option2) != m_ArgsMap.end())
        m_ErrorString += "Options --" + option1 + " and --" + option2 + " are incompatible.\n";
}

//------------------------------------------------------------------------------

void TCmdLineChecker::endCheck()
{
    for (TStringListMap::const_iterator Iter = m_ArgsMap.begin(); Iter != m_ArgsMap.end(); ++Iter)
        m_ErrorString += "Unknown option: --" + Iter->first + ".\n";

}

//------------------------------------------------------------------------------

string TCmdLineChecker::check(const TStringListMap& argsMap)
{
    TCmdLineChecker Checker(argsMap);

    Checker.checkIncompatible("backup", "nobackup");
    Checker.check("version",  otNoValue);
    Checker.check("help",     otNoValue);
    Checker.check("verbose",  otNoValue);
    Checker.check("logfile",  otSingleValue);
    Checker.check("backup",   otNoValue);
    Checker.check("nobackup", otNoValue);
    Checker.check("force",    otNoValue);
    Checker.check("qt-dir",   otSingleValue);
    Checker.check("new-dir",  otSingleValue);
    Checker.check("old-dir",  otMultyValue);
    Checker.endCheck();

    return Checker.m_ErrorString;
}

//------------------------------------------------------------------------------

void TCmdLineChecker::howToUseMessage()
{
    printf(
        //.......|.........|.........|.........|.........|.........|.........|.........|
        "Usage: qtbinpatcher [options]\n"
        "Options:\n"
        "  --version      Show program version and exit.\n"
        "  --help         Show this help and exit.\n"
        "  --verbose      Print extended runtime information.\n"
        "  --logfile=name Duplicate messages into logfile with name \"name\".\n"
        "  --backup       Create and save backup for files that'll be patched.\n"
        "  --nobackup     Don't create backup files in patch process.\n"
        "                 This option incompatible with option \"--backup\".\n"
        "                 WARNING: If an error occurs during operation, Qt library\n"
        "                          can be permanently damaged!\n"
        "    If missing --backup and no --nobackup, the backup files will be deleted"
        "    after successful completion of the operation or restored if an error occurs.\n"
        "  --force        Force patching (without old path actuality checking).\n"
        "  --qt-dir=path  Directory, where Qt or qmake is now located (may be relative).\n"
        "                 If not specified, patcher will try to find the file itself.\n"
        "  --new-dir=path Directory where Qt will be located (may be relative).\n"
        "                 If not specified, will be used the current location.\n"
        "  --old-dir=path Directory where Qt was located. This option can be specified\n"
        "                 more then once. The path will be replaced only in text files.\n"
        "\n"
        //.......|.........|.........|.........|.........|.........|.........|.........|
    );
}

//------------------------------------------------------------------------------
