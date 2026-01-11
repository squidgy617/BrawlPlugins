#pragma once

#include "sel_char_load_thread.h"

class CoreApi;
namespace CSSHooks {
    void InstallHooks(CoreApi* api);
    bool isLoaded(u32 resLoader);
    void _returnAddr();
    void _placeholderTextureReturn();
    void _placeholderTextureSkip();
    void _logStuffReturn();
    void _materialChangeSkip();
    void _materialChangeReturn();
} // namespace CSSHooks
