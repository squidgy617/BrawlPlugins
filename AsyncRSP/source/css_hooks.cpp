#include <CX.h>
#include <OS/OSCache.h>
#include <os/OSError.h>
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
        if (data == NULL) {
            data = selCharArchive->getData(Data_Type_Misc, 0, 0xfffe);
        }
        // If CSP is in archive mark as loaded
        else {
            thread->imageLoaded();
        }


        // if the CSP is not in the archive request to load the RSP instead
        if (thread->getLoadedCharKind() != charKind)
        {
            thread->requestLoad(charKind);
        }
        // If character is already loaded mark as such
        else if (area->m_charKind != charKind) {
            thread->imageLoaded();
        }
        if (!thread->isReady()) {
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

    void blankImage(MuObject* charPic)
    {
        charPic->changeMaterialTex(1,"MenSelchrFaceB.501");
        charPic->changeMaterialTex(0,"MenSelchrFaceB.501");
    }

    void leavingSlot()
    {
        register muSelCharPlayerArea* area;
        asm
        {
            mr area, r30;
        }
        blankImage(area->m_muCharPic);

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

    void changeFranchiseIcon() {
        register muSelCharPlayerArea* area;
        register int chrKind;
        // Pull vars from registers
        asm
        {
            mr area, r30;
            mr chrKind, r31;
        }
        selCharLoadThread* thread = selCharLoadThread::getThread(area->m_areaIdx);
        // If regular char, only update once RSP is loaded
        if (thread->updateEmblem() || thread->isNoLoadSelchKind(chrKind)) {
            area->dispMarkKind((MuSelchkind)chrKind);
            thread->emblemUpdated();
        }
    }

    void changeName() {
        // setFrameTex is called after this using the frame we determine here
        register muSelCharPlayerArea* area;
        register int chrKind;
        register float frameIndex;
        register float newFrameIndex;
        // Pull vars from registers
        asm
        {
            mr area, r30;
            mr chrKind, r31;
            fsubs frameIndex, f0, f1;
        }
        selCharLoadThread* thread = selCharLoadThread::getThread(area->m_areaIdx);
        // If RSP is not ready, keep displaying the current frame
        if (!thread->updateName() && !thread->isNoLoadSelchKind(chrKind)) {
            newFrameIndex = area->m_muCharName->m_modelAnim->getFrame();
        }
        else {
            newFrameIndex = frameIndex;
            thread->nameUpdated();
        }
        // Otherwise, load the new frame in
        asm{
            fsubs f1, f0, newFrameIndex;
        }
    }

    asm void clearFranchiseIcons() {
        nofralloc
        cmpwi r31, 0x29	// if random, continue to call setFrameTex
        beq setFrameTex
        cmpwi r31, 0x28	// if none, continue to call setFrameTex
        beq setFrameTex

        lis r12, 0x8069			// otherwise, skip setFrameTex
        ori r12, r12, 0x70dc
        mtctr r12
        bctrl
        
        setFrameTex:
        lwz r3, 0x00B8 (r30) // original instruction

        lis r12, 0x8069			// return
        ori r12, r12, 0x70d8
        mtctr r12
        bctrl
    }

    void onModuleLoaded(gfModuleInfo* info)
    {
        gfModuleHeader* header = info->m_module->header;
        Modules::_modules moduleID = static_cast<Modules::_modules>(header->id);
        u32 textAddr = header->getTextSectionAddr();
        u32 writeAddr;

        if (moduleID == Modules::SORA_MENU_SEL_CHAR)
        {  
            writeAddr = 0x806C8734; //Yeah I didn't bother to do anything fancy here since it is in a different module
            *(u32*)writeAddr = 0x3CE00021; //lis r7, 0x21 806C8734. Originally lis r7, 0x10. Related to memory allocated for the entire CSS.
        }
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

        // hook to clear CSPs when no fighter is selected
        api->syInlineHookRel(0x14BC8, reinterpret_cast<void*>(leavingSlot), Modules::SORA_MENU_SEL_CHAR);

        // hook to change franchise icon when portrait loads
        api->syInlineHookRel(0x00014D98, 
                            reinterpret_cast<void*>(changeFranchiseIcon),
                            Modules::SORA_MENU_SEL_CHAR);

        // subscribe to onModuleLoaded event
        api->moduleLoadEventSubscribe(static_cast<SyringeCore::ModuleLoadCB>(onModuleLoaded));

        // hook to change name when portrait loads
        api->syInlineHookRel(0x00014D90, 
                            reinterpret_cast<void*>(changeName),
                            Modules::SORA_MENU_SEL_CHAR);

        // hook to clear existing franchise icon behavior
        api->sySimpleHookRel(0x00014810,
                            reinterpret_cast<void*>(clearFranchiseIcons),
                            Modules::SORA_MENU_SEL_CHAR);

    }
} // namespace CSSHooks
