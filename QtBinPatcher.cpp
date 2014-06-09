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

#include "QtBinPatcher.hpp"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <vector>
#include <algorithm>

#include "Logger.hpp"
#include "Functions.hpp"
#include "CmdLineParser.hpp"
#include "CmdLineChecker.hpp"
#include "QMake.hpp"
#include "Backup.hpp"

//------------------------------------------------------------------------------

using namespace std;
using namespace Functions;

//------------------------------------------------------------------------------

#define QT_PATH_MAX_LEN 450

//------------------------------------------------------------------------------

#ifdef OS_WINDOWS
// Case-insensitive comparision (only for case-independent file systems).

bool caseInsensitiveComp(const char c1, const char c2)
{
    return tolower(c1) == tolower(c2);
}

#endif

//------------------------------------------------------------------------------
// String comparision. For OS Windows comparision is case-insensitive.

bool strneq(const string& s1, const string& s2)
{
    #ifdef OS_WINDOWS
        string _s1 = s1, _s2 = s2;
        transform(_s1.begin(), _s1.end(), _s1.begin(), ::tolower);
        transform(_s2.begin(), _s2.end(), _s2.begin(), ::tolower);
        return _s1 != _s2;
    #else
        return s1 != s2;
    #endif
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

bool TQtBinPatcher::parseArgs(int argc, const char* argv[])
{
    if (!m_CmdLineParser.parse(argc, argv)) {
        LOG(m_CmdLineParser.errorString().c_str());
        TCmdLineChecker::howToUseMessage();
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::checkArgs()
{
    string ErrorString = TCmdLineChecker::check(m_CmdLineParser.argsMap());
    if (!ErrorString.empty()) {
        LOG("%s\n", ErrorString.c_str());
        TCmdLineChecker::howToUseMessage();
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::getQtDir()
{
    m_QtDir = m_CmdLineParser.value("qt-dir");
    if (!m_QtDir.empty())
        m_QtDir = toNormalSeparators(absolutePath(m_QtDir));

    if (!m_QMake.find(m_QtDir)) {
        LOG_E("Can't find qmake.\n");
        return false;
    }
    else {
        LOG_V("Path to qmake: \"%s\".\n", m_QMake.qmakePath().c_str());
    }

    if (m_QtDir.empty()) {
        m_QtDir = m_QMake.qtPath();
        if (m_QtDir.empty()) {
            LOG_E("Can't determine path to Qt directory.\n");
            return false;
        }
    }

    m_QtDir = toNormalSeparators(m_QtDir);
    LOG_V("Path to Qt directory: \"%s\".\n", m_QtDir.c_str());
    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::getNewQtDir()
{
    m_NewQtDir = m_CmdLineParser.value("new-dir");
    if (!m_NewQtDir.empty())
        m_NewQtDir = absolutePath(m_NewQtDir);
    else
        m_NewQtDir = m_QtDir;
    m_NewQtDir = toNormalSeparators(m_NewQtDir);
    LOG_V("Path to new Qt directory: \"%s\".\n", m_NewQtDir.c_str());

    if (m_NewQtDir.length() > QT_PATH_MAX_LEN) {
        LOG_E("Path to new Qt directory too long (%i symbols).\n"
              "Path must be not longer as %i symbols.",
            m_NewQtDir.length(), QT_PATH_MAX_LEN);
        return false;
    }
    return !m_NewQtDir.empty();
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::isPatchNeeded()
{
    assert(hasOnlyNormalSeparators(m_NewQtDir));

    string OldQtDir = toNormalSeparators(m_QMake.qtInstallPrefix());
    if (!OldQtDir.empty() && !m_NewQtDir.empty())
        return strneq(OldQtDir, m_NewQtDir);
    return false;
}

//------------------------------------------------------------------------------

void TQtBinPatcher::addTxtPatchValues(const string& oldPath)
{
    assert(hasOnlyNormalSeparators(oldPath));

    if (!oldPath.empty()) {
        m_TxtPatchValues[oldPath] = m_NewQtDir;
        string S = oldPath;
        replace(S.begin(), S.end(), '/', '\\');
        m_TxtPatchValues[S] = m_NewQtDir;

        #ifdef OS_WINDOWS
            string NewQtDirDS = m_NewQtDir;
            replace(&NewQtDirDS, '/', "\\\\");
            S = oldPath;
            replace(&S, '/', "\\\\");
            m_TxtPatchValues[S] = NewQtDirDS;
        #endif
    }
}

//------------------------------------------------------------------------------

void TQtBinPatcher::createBinPatchValues()
{
    struct TParam {
        const char* const Name;
        const char* const Prefix;
        const char* const Dir;
    };

    static const TParam Params[] = {
        { "QT_INSTALL_PREFIX",       "qt_prfxpath=", NULL           },
        { "QT_INSTALL_ARCHDATA",     "qt_adatpath=", NULL           },
        { "QT_INSTALL_DOCS",         "qt_docspath=", "doc"          },
        { "QT_INSTALL_HEADERS",      "qt_hdrspath=", "include"      },
        { "QT_INSTALL_LIBS",         "qt_libspath=", "lib"          },
        { "QT_INSTALL_LIBEXECS",     "qt_lbexpath=", "libexec"      },
        { "QT_INSTALL_BINS",         "qt_binspath=", "bin"          },
        { "QT_INSTALL_PLUGINS",      "qt_plugpath=", "plugins"      },
        { "QT_INSTALL_IMPORTS",      "qt_impspath=", "imports"      },
        { "QT_INSTALL_QML",          "qt_qml2path=", "qml"          },
        { "QT_INSTALL_DATA",         "qt_datapath=", NULL           },
        { "QT_INSTALL_TRANSLATIONS", "qt_trnspath=", "translations" },
        { "QT_INSTALL_EXAMPLES",     "qt_xmplpath=", "examples"     },
        { "QT_INSTALL_DEMOS",        "qt_demopath=", "demos"        },
        { "QT_INSTALL_TESTS",        "qt_tstspath=", "tests"        },
        { "QT_HOST_PREFIX",          "qt_hpfxpath=", NULL           },
        { "QT_HOST_BINS",            "qt_hbinpath=", "bin"          },
        { "QT_HOST_DATA",            "qt_hdatpath=", NULL           },
        { "QT_HOST_LIBS",            "qt_hlibpath=", "lib"          }
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    string newQtDirNative = toNativeSeparators(m_NewQtDir);
    for (size_t i = 0; i < sizeof(Params)/sizeof(Params[0]); ++i)
    {
        const TParam& Param = Params[i];
        string OldValue = m_QMake.value(Param.Name);
        if (!OldValue.empty()) {
            OldValue.insert(0, Param.Prefix);
            string NewValue = Param.Prefix;
            NewValue.append(newQtDirNative);
            if (Param.Dir != NULL) {
                NewValue += nativeSeparator();
                NewValue += Param.Dir;
            }
            m_BinPatchValues[OldValue] = NewValue;
        }
        else {
            LOG_V("Variable \"%s\" not found in qmake output.\n", Param.Name);
        }
    }
}

//------------------------------------------------------------------------------

void TQtBinPatcher::createPatchValues()
{
    m_TxtPatchValues.clear();
    m_BinPatchValues.clear();

    addTxtPatchValues(toNormalSeparators(m_QMake.qtInstallPrefix()));
    createBinPatchValues();

    const TStringList* pValues = m_CmdLineParser.values("old-dir");
    if (pValues != NULL)
        for (TStringList::const_iterator Iter = pValues->begin(); Iter != pValues->end(); ++Iter)
            addTxtPatchValues(toNormalSeparators(*Iter));

    LOG_V("\nPatch values for text files:\n%s",
          stringMapToStr(m_TxtPatchValues, "  \"", "\" -> \"", "\"\n").c_str());

    LOG_V("\nPatch values for binary files:\n%s",
          stringMapToStr(m_BinPatchValues, "  \"", "\" -> \"", "\"\n").c_str());
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::createTxtFilesForPatchList()
{
    struct TElement {
        const char* const Dir;
        const char* const Name;
        const bool        Recursive;
    };

    // Files for patching in Qt4.
    static const TElement Elements4[] = {
        { "/lib/",             "*.prl",              false },
        { "/demos/shared/",    "libdemo_shared.prl", false },
#if defined(OS_WINDOWS)
        { "/mkspecs/default/", "qmake.conf",         false },
        { "/",                 ".qmake.cache",       false }
#elif defined(OS_LINUX)
        { "/lib/",             "*.la",               false },
        { "/lib/pkgconfig/",   "*.pc",               false },
        { "/mkspecs/",         "qconfig.pri",        false }
#endif
    };

    // Files for patching in Qt5.
    static const TElement Elements5[] = {
        { "/",                            "*.la",                         true  },
        { "/",                            "*.prl",                        true  },
        { "/",                            "*.pc",                         true  },
        { "/",                            "*.pri",                        true  },
        { "/lib/cmake/Qt5LinguistTools/", "Qt5LinguistToolsConfig.cmake", false },
        { "/mkspecs/default-host/",       "qmake.conf",                   false },
#ifdef OS_WINDOWS
        { "/mkspecs/default/",            "qmake.conf",                   false },
        { "/",                            ".qmake.cache",                 false },
        { "/lib/",                        "prl.txt",                      false }
#endif
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    m_TxtFilesForPatch.clear();

    const TElement* Elements;
    size_t Count;
    switch (m_QMake.qtVersion()) {
        case '4' :
            Elements = Elements4;
            Count = sizeof(Elements4)/sizeof(Elements4[0]);
            break;
        case '5' :
            Elements = Elements5;
            Count = sizeof(Elements5)/sizeof(Elements5[0]);
            break;
        default :
            LOG_E("Unsupported Qt version (%c).", m_QMake.qtVersion());
            return false;
    }
    for (size_t i = 0; i < Count; ++i) {
        if (Elements[i].Recursive)
            splice(&m_TxtFilesForPatch, findFilesRecursive(m_QtDir + Elements[i].Dir, Elements[i].Name));
        else
            splice(&m_TxtFilesForPatch, findFiles(m_QtDir + Elements[i].Dir, Elements[i].Name));
    }

    LOG_V("\nList of text files for patch:\n%s\n",
          stringListToStr(m_TxtFilesForPatch, "  ", "\n").c_str());

    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::createBinFilesForPatchList()
{
    struct TElement {
        const char* const Dir;
        const char* const Name;
    };

    // Files for patching in Qt4.
    static const TElement Elements4[] = {
#if defined(OS_WINDOWS)
        { "/bin/", "qmake.exe"    },
        { "/bin/", "lrelease.exe" },
        { "/bin/", "QtCore*.dll"  },
        { "/lib/", "QtCore*.dll"  }
#elif defined(OS_LINUX)
        { "/bin/", "qmake"        },
        { "/bin/", "lrelease"     },
        { "/lib/", "libQtCore.so" }
#endif
    };

    // Files for patching in Qt5.
    static const TElement Elements5[] = {
#if defined(OS_WINDOWS)
        { "/bin/", "qmake.exe"    },
        { "/bin/", "lrelease.exe" },
        { "/bin/", "qdoc.exe"     },
        { "/bin/", "Qt5Core*.dll" },
        { "/lib/", "Qt5Core*.dll" }
#elif defined(OS_LINUX)
        { "/bin/", "qmake"        },
        { "/bin/", "lrelease"     },
        { "/bin/", "qdoc"         },
        { "/lib/", "libQtCore.so" }
#endif
    };

    // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

    m_BinFilesForPatch.clear();

    const TElement* Elements;
    size_t Count;
    switch (m_QMake.qtVersion()) {
        case '4' :
            Elements = Elements4;
            Count = sizeof(Elements4)/sizeof(Elements4[0]);
            break;
        case '5' :
            Elements = Elements5;
            Count = sizeof(Elements5)/sizeof(Elements5[0]);
            break;
        default :
            LOG_E("Unsupported Qt version (%c).", m_QMake.qtVersion());
            return false;
    }

    for (size_t i = 0; i < Count; ++i)
        splice(&m_BinFilesForPatch, findFiles(m_QtDir + Elements[i].Dir, Elements[i].Name));

    LOG_V("\nList of binary files for patch:\n%s\n",
          stringListToStr(m_BinFilesForPatch, "  ", "\n").c_str());

    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::patchTxtFile(const string& fileName)
{
    LOG("Patching text file \"%s\".\n", fileName.c_str());

    bool Result = false;

    FILE* File = fopen(fileName.c_str(), "r+b");
    if (File!= NULL)
    {
        vector<char> Buf;
        long FileLength = getFileSize(File);

        if (FileLength > 0)
        {
            Buf.resize(FileLength);
            if (fread(Buf.data(), FileLength, 1, File) == 1) // TODO: C++11 requred!
            {
                for (TStringMap::const_iterator Iter = m_TxtPatchValues.begin(); Iter != m_TxtPatchValues.end(); ++Iter)
                {
                    string::size_type Delta = 0;
                    vector<char>::iterator Found;
                    while ((Found = search(Buf.begin() + Delta, Buf.end(),
                                           Iter->first.begin(), Iter->first.end()
                                           #ifdef OS_WINDOWS
                                               , caseInsensitiveComp
                                           #endif
                            ))
                           != Buf.end())
                    {
                        Delta = Found - Buf.begin() + static_cast<int>(Iter->second.length());
                        Found = Buf.erase(Found, Found + Iter->first.length());
                        Buf.insert(Found, Iter->second.begin(), Iter->second.end());
                    }
                }
                zeroFile(File);
                if (fwrite(Buf.data(), Buf.size(), 1, File) == 1)
                    Result = true;
                else
                    LOG_E("Error writing to file \"%s\".\n", fileName.c_str());
            }
            else {
                LOG_E("Error reading from file \"%s\".\n", fileName.c_str());
            }
        }
        else {
            LOG_V("  File is empty. Skipping.\n");
            Result = true;
        }

        fclose(File);
    }
    else {
        LOG_E("Error opening file \"%s\". Error %i.\n", fileName.c_str(), errno);
    }

    return Result;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::patchBinFile(const string& fileName)
{
    LOG("Patching binary file \"%s\".\n", fileName.c_str());

    bool Result = false;

    FILE* File = fopen(fileName.c_str(), "r+b");
    if (File != NULL)
    {
        long BufSize = getFileSize(File);
        char* Buf = new char[BufSize];

        if (fread(Buf, BufSize, 1, File) == 1)
        {
            for (TStringMap::const_iterator Iter = m_BinPatchValues.begin(); Iter != m_BinPatchValues.end(); ++Iter)
            {
                char* First = Buf;
                while ((First = search(First, Buf + BufSize,
                                       Iter->first.begin(), Iter->first.end()))
                       != Buf + BufSize)
                {
                    strcpy(First, Iter->second.c_str());
                    First += Iter->second.length();
                    int Delta = static_cast<int>(Iter->first.length()) -
                                static_cast<int>(Iter->second.length());
                    if (Delta > 0) {
                        memset(First, 0, Delta);
                        First += Delta;
                    }
                }
            }
            rewind(File);
            if (fwrite(Buf, BufSize, 1, File) == 1)
                Result = true;
            else
                LOG_E("Error writing to file \"%s\".\n", fileName.c_str());
        }
        else {
            LOG_E("Error reading from file \"%s\".\n", fileName.c_str());
        }

        delete Buf;
        fclose(File);
    }
    else {
        LOG_E("Error opening file \"%s\". Error %i.\n", fileName.c_str(), errno);
    }

    return Result;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::patchTxtFiles()
{
    for (TStringList::const_iterator Iter = m_TxtFilesForPatch.begin(); Iter != m_TxtFilesForPatch.end(); ++Iter)
        if (!patchTxtFile(*Iter))
            return false;
    return true;
}

//------------------------------------------------------------------------------

bool TQtBinPatcher::patchBinFiles()
{
    for (TStringList::const_iterator Iter = m_BinFilesForPatch.begin(); Iter != m_BinFilesForPatch.end(); ++Iter)
        if (!patchBinFile(*Iter))
            return false;
    return true;
}

//------------------------------------------------------------------------------

TQtBinPatcher::TQtBinPatcher()
{
    LOG("\n"
        "QtBinPatcher v2.0.0. Tool for patching paths in Qt binaries.\n"
        "Yuri V. Krugloff, 2013-2014. http://www.tver-soft.org\n"
        "This is free software released into the public domain.\n\n");

}

//------------------------------------------------------------------------------

int TQtBinPatcher::exec(int argc, const char* argv[])
{
    // Initialization.
    if (!parseArgs(argc, argv)) return -1;
    if (!checkArgs()) return -1;

    TLogger::setVerbose(m_CmdLineParser.contains("verbose"));
    LOG_SET_FILENAME(m_CmdLineParser.value("logfile").c_str());
    LOG_V(m_CmdLineParser.dump().c_str());

    if (m_CmdLineParser.contains("help")) {
        TCmdLineChecker::howToUseMessage();
        return 0;
    }

    if (m_CmdLineParser.contains("version"))
        return 0;

    // Work.
    if (!getQtDir()) return -1;
    if (!getNewQtDir()) return -1;

    TBackup Backup;
    Backup.setSkipBackup(m_CmdLineParser.contains("nobackup"));
    if (!Backup.backupFile(m_QtDir + "/bin/qt.conf", TBackup::bmRename)) return -1;

    if (!m_QMake.query()) return -1;
    if (!m_QMake.parse()) return -1;
    if (!isPatchNeeded()) {
        if (m_CmdLineParser.contains("force")) {
            LOG("\nThe new and the old pathes to Qt directory are the same.\n"
                "Perform forced patching.\n\n");
        }
        else {
            LOG("\nThe new and the old pathes to Qt directory are the same.\n"
                "Patching not needed.\n");
            return 0;
        }
    }

    createPatchValues();
    if (!createTxtFilesForPatchList()) return -1;
    if (!createBinFilesForPatchList()) return -1;

    if (!Backup.backupFiles(m_TxtFilesForPatch)) return -1;
    if (!Backup.backupFiles(m_BinFilesForPatch)) return -1;
    if (!patchTxtFiles()) return -1;
    if (!patchBinFiles()) return -1;

    // Finalization.
    if (m_CmdLineParser.contains("backup"))
        Backup.save();
    else
        if (!Backup.deleteBackup())
            return -1;

    return 0;
}

//------------------------------------------------------------------------------
