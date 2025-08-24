#pragma once

#include "sel_char_load_thread.h"

class CoreApi;
namespace CSSHooks {
    void InstallHooks(CoreApi* api);
    bool isLoaded(u32 resLoader);
    void _returnAddr();
} // namespace CSSHooks
