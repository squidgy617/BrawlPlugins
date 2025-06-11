#include <CX.h>
#include <OS/OSCache.h>
#include <gf/gf_archive.h>
#include <memory.h>
#include <modules.h>
#include <mu/menu.h>
#include <mu/selchar/mu_selchar_player_area.h>
#include <nw4r/g3d/g3d_resfile.h>
#include <sy_core.h>
#include <types.h>
#include <vector.h>
#include "css_hooks.h"

using namespace nw4r::g3d;

namespace CSSHooks {

    extern gfArchive* selCharArchive;

    void createThreads(muSelCharPlayerArea* area)
    {
        selCharLoadThread* thread = new (Heaps::MenuResource) selCharLoadThread(area);
    }

    // NOTE: This hook gets triggered again by the load thread since
    // the thread calls `setCharPic` when data is finished loading
    ResFile* getCharPicTexResFile(register muSelCharPlayerArea* area, u32 charKind)
    {
        selCharLoadThread* thread = selCharLoadThread::getThread(area->m_areaIdx);

        // Handles conversions for poketrio and special slots
        int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
        id = muMenu::getStockFrameID(id);

        // check if CSP exists in archive first.
        void* data = selCharArchive->getData(Data_Type_Misc, id, 0xfffe);


        // if the CSP is not in the archive request to load the RSP instead
        if (thread->getLoadedCharKind() != charKind)
        {
            thread->requestLoad(charKind);
        }
        if (!thread->isTargetPortraitReady(charKind)) {
            CXUncompressLZ(data, area->m_charPicData);
            // flush cache
            DCFlushRange(area->m_charPicData, 0x40000);

            // set ResFile to point to filedata
            area->m_charPicRes = ResFile(area->m_charPicData);

            // init resFile and return
            ResFile::Init(&area->m_charPicRes);

            return &area->m_charPicRes;
        }
        else {
            // if the CSP data is in the archive, load the data from there
            void* buffer = thread->getBuffer();
            // copy data from temp load buffer
            memcpy(area->m_charPicData, buffer, 0x40000);

            DCFlushRange(area->m_charPicData, 0x40000);

            // set ResFile to point to filedata
            area->m_charPicRes = ResFile(area->m_charPicData);

            // init resFile and return
            ResFile::Init(&area->m_charPicRes);

            return &area->m_charPicRes;
        }
    }

    void (*_loadCharPic)(void*);
    void loadCharPic(muSelCharPlayerArea* area) {
        selCharLoadThread::getThread(area->m_areaIdx)->main();
        _loadCharPic(area);
    }

    muSelCharPlayerArea* (*_destroyPlayerAreas)(void*, int);
    muSelCharPlayerArea* destroyPlayerAreas(muSelCharPlayerArea* object, int external)
    {
        // destroy our load thread
        delete selCharLoadThread::getThread(object->m_areaIdx);

        return _destroyPlayerAreas(object, external);
    }

    void InstallHooks(CoreApi* api)
    {
        // hook to load portraits from RSPs
        api->syReplaceFuncRel(0x1107c,
                              reinterpret_cast<void*>(getCharPicTexResFile),
                              NULL,
                              Modules::SORA_MENU_SEL_CHAR);

        // hook to clean up our mess when unloading CSS
        api->syReplaceFuncRel(0x10EF8,
                              reinterpret_cast<void*>(destroyPlayerAreas),
                              (void**)&_destroyPlayerAreas,
                              Modules::SORA_MENU_SEL_CHAR);

        api->syReplaceFuncRel(0x00012210,
                              reinterpret_cast<void*>(loadCharPic),
                              (void**)&_loadCharPic,
                              Modules::SORA_MENU_SEL_CHAR);


        // hook to create threads when booting the CSS
        api->syInlineHookRel(0x3524, reinterpret_cast<void*>(createThreads), Modules::SORA_MENU_SEL_CHAR);

    }
} // namespace CSSHooks
