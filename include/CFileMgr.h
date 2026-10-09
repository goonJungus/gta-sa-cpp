// CFileMgr - adapted from gta-reversed for clean-room C++ build
// Source: gta-reversed/source/game_sa/FileMgr.h
// (https://github.com/gta-reversed/gta-reversed)
// Original authors: GTA Community (plugin-sdk contributors)
// This file is part of a clean-room engine reimplementation for interoperability research.
// Game assets are loaded from the user's own install at runtime and never shipped.
//
// Adaptations:
// - stripped InjectHooks() and the InjectHooksMain friend (no hook system in clean-room)
// - StaticRef<T>(addr) statics -> static member declarations; definitions live in
//   src/CFileMgr.cpp

#pragma once

#include "GrTypes.h" // int8..uint32

#include <cstddef> // size_t
#include <cstdio>

// For backward compatibility
typedef FILE* FILESTREAM;
constexpr size_t DIRNAMELENGTH = 128;

static constexpr auto TopLineEmptyFile{ "THIS FILE IS NOT VALID YET" };

class CFileMgr {
public:
    static char ms_dirName[DIRNAMELENGTH];
    static char ms_rootDirName[DIRNAMELENGTH];

    static void Initialise();
    static int32 ChangeDir(const char* path);
    static int32 SetDir(const char* path);
    static int32 SetDirMyDocuments();
    static size_t LoadFile(const char* path, uint8* buf, size_t size, const char* mode);
    static FILESTREAM OpenFile(const char* path, const char* mode);
    static FILESTREAM OpenFileForWriting(const char* path);
    static FILESTREAM OpenFileForAppending(const char* path);
    static size_t Read(FILESTREAM file, void* buf, size_t size);
    static size_t Write(FILESTREAM file, const void* buf, size_t size);
    static bool Seek(FILESTREAM file, long offset, int32 origin);
    static bool ReadLine(FILESTREAM file, char* str, int32 num);
    static int32 CloseFile(FILESTREAM file);
    static int32 GetTotalSize(FILESTREAM file);
    static int32 Tell(FILESTREAM file);
    static bool GetErrorReadWrite(FILESTREAM file);

    //! Increment the file's seek pointer until after the next new line (`\n`) (make sure file is open in non-binary mode!)
    static void SeekNextLine(FILESTREAM file);
};

char* InitUserDirectories();
