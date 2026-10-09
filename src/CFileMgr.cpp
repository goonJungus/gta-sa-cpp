// CFileMgr - adapted from gta-reversed for clean-room C++ build
// Method bodies ported from decompiled: src/CFileMgr/*.c
//   Initialise_005386f0, ChangeDir_00538730, SetDir_005387d0,
//   SetDirMyDocuments_00538860, LoadFile_00538890, Read_00538950,
//   Write_00538970, Seek_00538990, ReadLine_005389b0, Tell_00538a20,
//   GetErrorReadWrite_00538a50, GetTotalSize_005389e0,
//   OpenFileForWriting_00538910, OpenFileForAppending_00538930
//
// Conversion notes:
// - Ghidra types mapped: undefined4->uint32, undefined2->uint16, byte->uint8,
//   uint->uint32, BOOL->bool. __cdecl stripped.
// - The decompiled file ops go through RenderWare's file-function table
//   (RwErrorGet() + indirect call); the bodies are plain CRT equivalents
//   (fopen/fread/fwrite/fseek/ftell/fgets/fclose/feof/ferror) - same as the
//   gta-reversed port, which notes the RW table is skipped for portability.
// - Read-only .rdata string globals inlined: &DAT_00860adc -> "wb",
//   &DAT_00863a2c -> "a".
// - ChangeDir/SetDir tail (SetCurrentDirectoryA + per-drive "=X:" env sync):
//   the decompiled inlines it in ChangeDir and shares it via unk_00836f1e in
//   SetDir/SetDirMyDocuments; ported as one static helper ApplyDirChange().
// - InitUserDirectories() has no exported decompiled body (it lives at
//   0x744FB0, outside src/CFileMgr/); implemented clean-room from the
//   documented behavior (CSIDL_PERSONAL + "\\GTA San Andreas User Files").

#include "CFileMgr.h"

#include <cctype>  // toupper
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h> // SHGetFolderPathA, CSIDL_PERSONAL
#endif

// StaticRef<char[128]>(0xb71a60) -> plain statics (see header)
char CFileMgr::ms_dirName[DIRNAMELENGTH]{};
char CFileMgr::ms_rootDirName[DIRNAMELENGTH]{};

// Shared tail of ChangeDir/SetDir/SetDirMyDocuments.
// Decompiled: inlined at the end of ChangeDir_00538730, shared via
// unk_00836f1e by SetDir_005387d0 / SetDirMyDocuments_00538860.
// Applies ms_dirName, then keeps the CRT per-drive directory ("=X:") in sync
// unless the new directory is a UNC root. Returns 0 on success, -1 on failure
// (the decompiled maps the Win32 error via __dosmaperr before returning -1).
static int32 ApplyDirChange() {
#ifdef _WIN32
    if (!SetCurrentDirectoryA(CFileMgr::ms_dirName))
        return -1;
    char currDir[MAX_PATH];
    if (!GetCurrentDirectoryA(sizeof(currDir), currDir))
        return -1;
    // Skip the env sync for UNC roots ("\\" / "//" - both leading chars equal).
    if ((currDir[0] != '\\' && currDir[0] != '/') || currDir[0] != currDir[1]) {
        const char envName[4] = { '=', (char)toupper((unsigned char)currDir[0]), ':', '\0' };
        if (!SetEnvironmentVariableA(envName, currDir))
            return -1;
    }
    return 0;
#else
    // TODO(port): non-Windows equivalent of the SetCurrentDirectory bookkeeping.
    (void)CFileMgr::ms_dirName;
    return -1;
#endif
}

// 0x5386F0
void CFileMgr::Initialise() {
    memset(ms_rootDirName, 0, sizeof(ms_rootDirName));
    // Append a trailing backslash to ms_dirName (preset by the caller from the
    // executable path before Initialise runs).
    const size_t len = strlen(ms_dirName);
    if (len + 2 <= sizeof(ms_dirName)) {
        ms_dirName[len] = '\\';
        ms_dirName[len + 1] = '\0';
    }
}

// 0x538730
int32 CFileMgr::ChangeDir(const char* path) {
    if (path[0] == '\\') {
        strcpy(ms_dirName, ms_rootDirName);
        ++path;
    }
    if (path[0] != '\0') {
        strcat(ms_dirName, path);
        const size_t len = strlen(ms_dirName);
        if (ms_dirName[len - 1] != '\\' && len + 2 <= sizeof(ms_dirName)) {
            ms_dirName[len] = '\\';
            ms_dirName[len + 1] = '\0';
        }
    }
    return ApplyDirChange();
}

// 0x5387D0
int32 CFileMgr::SetDir(const char* path) {
    strcpy(ms_dirName, ms_rootDirName);
    if (path[0] != '\0') {
        strcat(ms_dirName, path);
        const size_t len = strlen(ms_dirName);
        if (ms_dirName[len - 1] != '\\' && len + 2 <= sizeof(ms_dirName)) {
            ms_dirName[len] = '\\';
            ms_dirName[len + 1] = '\0';
        }
    }
    return ApplyDirChange();
}

// 0x538860
int32 CFileMgr::SetDirMyDocuments() {
    strcpy(ms_dirName, InitUserDirectories());
    return ApplyDirChange();
}

// 0x538890
// NOTE: the original ignores the `size` parameter: it reads in 0x4000 chunks
// until a short read and NUL-terminates past the payload (callers over-allocate
// by one). Ported faithfully; see gta-reversed NOTSA note about the overflow.
size_t CFileMgr::LoadFile(const char* path, uint8* buf, size_t size, const char* mode) {
    (void)size; // ignored by the original - see note above
    FILE* file = fopen(path, mode);
    if (!file)
        return (size_t)-1;
    size_t total = 0;
    size_t n;
    do {
        n = fread(buf + total, 1, 0x4000, file);
        total += n;
    } while (n == 0x4000);
    buf[total] = '\0';
    fclose(file);
    return total;
}

// 0x538900 (inlined into LoadFile in the decompiled export set)
FILESTREAM CFileMgr::OpenFile(const char* path, const char* mode) {
    return fopen(path, mode);
}

// 0x538910
FILESTREAM CFileMgr::OpenFileForWriting(const char* path) {
    return OpenFile(path, "wb");
}

// 0x538930
FILESTREAM CFileMgr::OpenFileForAppending(const char* path) {
    return OpenFile(path, "a");
}

// 0x538950
size_t CFileMgr::Read(FILESTREAM file, void* buf, size_t size) {
    return fread(buf, 1, size, file);
}

// 0x538970
size_t CFileMgr::Write(FILESTREAM file, const void* buf, size_t size) {
    return fwrite(buf, 1, size, file);
}

// 0x538990
// NOTE: inverted sense is original - returns true when fseek FAILED.
bool CFileMgr::Seek(FILESTREAM file, long offset, int32 origin) {
    return fseek(file, offset, origin) != 0;
}

// 0x5389B0
bool CFileMgr::ReadLine(FILESTREAM file, char* str, int32 num) {
    return fgets(str, num, file) != nullptr;
}

// 0x5389D0 (CloseFile - no named export in src/CFileMgr/, trivial per header)
int32 CFileMgr::CloseFile(FILESTREAM file) {
    return fclose(file);
}

// 0x5389E0
int32 CFileMgr::GetTotalSize(FILESTREAM file) {
    const int32 currentPos = ftell(file);
    fseek(file, 0, SEEK_END);
    const int32 size = ftell(file);
    fseek(file, currentPos, SEEK_SET);
    return size;
}

// 0x538A20
int32 CFileMgr::Tell(FILESTREAM file) {
    return ftell(file);
}

// 0x538A50
// The decompiled reads the CRT FILE flag word directly ((_flag & _IOERR));
// ferror() is the portable equivalent.
bool CFileMgr::GetErrorReadWrite(FILESTREAM file) {
    return ferror(file) != 0;
}

// notsa: SeekNextLine
void CFileMgr::SeekNextLine(FILESTREAM file) {
    while (!feof(file) && fgetc(file) != '\n') {
        // spin to just past the next newline
    }
}

// 0x744FB0 - no decompiled body exported under src/CFileMgr/; clean-room
// implementation of the documented behavior: resolve "<Documents>\GTA San
// Andreas User Files", creating it (plus Gallery / User Tracks) on demand.
char* InitUserDirectories() {
    static char userDir[DIRNAMELENGTH]{};
    if (userDir[0] != '\0')
        return userDir;
#ifdef _WIN32
    char personal[MAX_PATH]{};
    if (SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, SHGFP_TYPE_CURRENT, personal) != S_OK) {
        strcpy(userDir, "data");
        return userDir;
    }
    snprintf(userDir, sizeof(userDir), "%s\\GTA San Andreas User Files", personal);
    CreateDirectoryA(userDir, nullptr);
    char subdir[MAX_PATH]{};
    snprintf(subdir, sizeof(subdir), "%s\\Gallery", userDir);
    CreateDirectoryA(subdir, nullptr);
    snprintf(subdir, sizeof(subdir), "%s\\User Tracks", userDir);
    CreateDirectoryA(subdir, nullptr);
#else
    // TODO(port): non-Windows user-documents location.
    strcpy(userDir, "data");
#endif
    return userDir;
}
