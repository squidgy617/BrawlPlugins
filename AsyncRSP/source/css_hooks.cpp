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
#include <ft/ft_manager.h>
#include <gf/gf_heap_manager.h>
#include <st/loader/st_loader_manager.h>
#include <st/loader/st_loader_player.h>
#include <gm/gm_global.h>

#define MEMORY_EXPANSION
// #define DEBUG

#ifdef MEMORY_EXPANSION
Heaps::HeapType fileHeap = Heaps::Fighter1Resource;
u32 bufferSize = 0xE0000;
#else
Heaps::HeapType fileHeap = Heaps::MenuResource;
u32 bufferSize = 0x40000;
#endif

using namespace nw4r::g3d;

namespace CSSHooks {

    extern gfArchive* selCharArchive;

    void createThreads(muSelCharPlayerArea* area)
    {
        Heaps::HeapType heap = fileHeap;
        selCharLoadThread* thread = new (heap) selCharLoadThread(area);
    }

    // Mark pac files to be deallocated from FighterResource heaps
    void deallocatePacs()
    {
        // Use state 5 to indicate PACs should deallocate, only works in conjunction with Dox's code... I think
        g_stLoaderManager->m_loaderPlayers[0]->m_state = 5;
        g_stLoaderManager->m_loaderPlayers[1]->m_state = 5;
        g_stLoaderManager->m_loaderPlayers[2]->m_state = 5;
        g_stLoaderManager->m_loaderPlayers[3]->m_state = 5;
    }

    // Check if FighterResource heaps need to be cleared and if so, clear them
    bool clearHeaps(u32 resLoader)
    {
        // Call isLoaded/[scResourceLoader]
        bool loaded = isLoaded(resLoader);

        // If we're ready to load, check that slots are ready to be cleared out
        if (loaded)
        {
            // Iterate through the slots
            for (int i = 0; i < 4; i++)
            {
                // State 5 indicates slot is marked for removal
                if (g_stLoaderManager->m_loaderPlayers[i]->m_state == 5)
                {
                    // Check slot is actually ready to be removed
                    if (g_ftManager->isReadyRemoveSlot(i))
                    {
                        // Remove slot and mark as removed
                        g_ftManager->removeSlot(i); // Clear slot data out of FighterResource
                        g_stLoaderManager->m_loaderPlayers[i]->m_slotId = -1;
                        g_stLoaderManager->m_loaderPlayers[i]->m_state = 0;
                    }
                    // Otherwise, loading should NOT continue
                    else
                    {
                        loaded = false;
                        break;
                    }
                }
            }
        }
        // Return whether we should continue loading or wait longer
        return loaded;
    }

    asm void __hookWrapper() {
        nofralloc;
        bl clearHeaps;
        b _returnAddr;
    }

    // These functions force a blank texture while the file loads - commented out to allow the previous portrait to continue display during load instead
    // void setPlaceholderTexture(int* id1, int* id2)
    // {
    //     register ResFile* resFile;

    //     asm
    //     {
    //         mr resFile, r26;
    //     }

    //     if (resFile->GetResTex("MenSelchrFaceB.501") != 0)
    //     {
    //         *id1 = 0;
    //         *id2 = 501;
    //     }
    // }

    // asm void __setPlaceholderTexture() {
    //     nofralloc
    //     mr r26, r3      // original instruction
    //     addi r3, r1, 0x48
    //     addi r4, r1, 0x4C
    //     stw r24, 0x48 (r1)
    //     stw r25, 0x4C (r1)
    //     bl setPlaceholderTexture
    //     lwz r24, 0x48(r1)
    //     lwz r25, 0x4C(r1)
    //     b _placeholderTextureReturn
    // }

    // These functions skip changing the portrait if a placeholder texture is found
    bool setPlaceholderTexture()
    {
        register ResFile* resFile;

        asm
        {
            mr resFile, r26;
        }

        return resFile->GetResTex("MenSelchrFaceB.501").ptr() != 0;
    }

    asm void __setPlaceholderTexture() {
        nofralloc
        mr r26, r3      // original instruction
        bl setPlaceholderTexture
        cmpwi r3, 0     // if we found our placeholder texture, skip rendering portrait
        bne skip
        b _placeholderTextureReturn     // otherwise, return and render portrait
        skip:
        b _placeholderTextureSkip
    }

    // These functions log information about portrait changes for debug purposes
    void logStuff(char* stringPtr)
    {
        register int param_1;

        asm
        {
            mr param_1, r31;
        }

        OSReport("Param 1: %d - String: %s - Frame: %f \n", param_1, stringPtr, g_GameGlobal->getGameFrame());
    }

    asm void __logStuff()
    {
        nofralloc
        addi r3, r1, 0x8
        bl logStuff
        addi r4,r29,0x374 // original instruction
        b _logStuffReturn
    }

    // NOTE: This hook gets triggered again by the load thread since
    // the thread calls `setCharPic` when data is finished loading
    ResFile* getCharPicTexResFile(register muSelCharPlayerArea* area, u32 charKind)
    {
        selCharLoadThread* thread = selCharLoadThread::getThread(area->m_areaIdx);

        // Handles conversions for poketrio and special slots
        int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
        id = muMenu::getStockFrameID(id);
        bool cspExists = false;

        // check if CSP exists in archive first.
        void* data = selCharArchive->getData(Data_Type_Misc, id, 0xfffe);
        if (data == NULL) {
            data = selCharArchive->getData(Data_Type_Misc, 50, 0xfffe);
        }
        // If CSP is in archive mark as loaded
        else {
            cspExists = true;
            thread->imageLoaded();
        }

        // if the CSP is not in the archive request to load the RSP instead
        if (thread->getLoadedCharKind() != charKind)
        {
            thread->requestLoad(charKind, cspExists);
        }
        // If character is already loaded mark as such
        else if (area->m_charKind != charKind) {
            thread->imageLoaded();
        }
        if (!thread->isReady()) {
            void* activeBuffer = thread->getActiveBuffer();
            CXUncompressLZ(data, activeBuffer);
            // flush cache
            DCFlushRange(activeBuffer, bufferSize);

            // set ResFile to point to filedata
            area->m_charPicRes = ResFile(activeBuffer);

            // init resFile and return
            ResFile::Init(&area->m_charPicRes);

            return &area->m_charPicRes;
        }
        else {
            // if the CSP data is in the archive, load the data from there
            void* fileBuffer = thread->getFileBuffer();
            void* activeBuffer = thread->getActiveBuffer();
            // copy data from temp load buffer
            memcpy(activeBuffer, fileBuffer, bufferSize);

            DCFlushRange(activeBuffer, bufferSize);

            // set ResFile to point to filedata
            area->m_charPicRes = ResFile(activeBuffer);

            // Swap which buffer is active
            thread->swapBuffers();

            // Mark image as loaded - thread will init resfile
            thread->imageLoaded();
            
            // Init resfile right away if we don't want a delay
            if (thread->shouldSkipDelay())
            {
                ResFile::Init(&area->m_charPicRes);
                thread->imageDisplayed();
                thread->materialUpdateStarted();
            }

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
        if (thread->shouldUpdateEmblem() || thread->isNoLoadSelchKind(chrKind)) {
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
        if (!thread->shouldUpdateName() && !thread->isNoLoadSelchKind(chrKind)) {
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

    bool getPortraitLoaded()
    {
        register muSelCharPlayerArea* area;
        register int chrKind;

        asm
        {
            mr area, r30;
            mr chrKind, r31;
        }

        selCharLoadThread* thread = selCharLoadThread::getThread(area->m_areaIdx);
        if (thread->shouldUpdateMaterial() || thread->isNoLoadSelchKind(chrKind))
        {
            thread->materialUpdated();
            return true;
        }
        return false;
    }

    asm void skipPortraitChange()
    {
        nofralloc
        mr r10, r3
        bl getPortraitLoaded
        cmpwi r3, 1             // check if portrait changed
        mr r3, r10
        mr r4, r3               // original instruction
        beq continueChange

        b _materialChangeSkip

        continueChange:
        b _materialChangeReturn
    }

    void onModuleLoaded(gfModuleInfo* info)
    {
        gfModuleHeader* header = info->m_module->header;
        Modules::_modules moduleID = static_cast<Modules::_modules>(header->id);
        u32 textAddr = header->getTextSectionAddr();
        u32 writeAddr;

        if (moduleID == Modules::SORA_MENU_SEL_CHAR)
        {  
            #ifdef MEMORY_EXPANSION
            writeAddr = 0x80693B10; // Change copying to happen in the Fighter2Resource heap
            *(u32*)writeAddr = 0x38600013; // 43 (MenuResource) -> 19 (Fighter2Resource)

            writeAddr = 0x80693B14; // Increase buffer size for portraits
            *(u32*)writeAddr = 0x3c80000E; // lis r4, 4 -> lis r4, 14

            writeAddr = 0x800E6068; // Change heap for RSP on result screen
            *(u32*)writeAddr = 0x38a00011; // li r5, 42 (MenuInstance) -> li r5, 17 (StageResource)
            #else
            writeAddr = 0x806C8734; //Yeah I didn't bother to do anything fancy here since it is in a different module
            *(u32*)writeAddr = 0x3CE00021; //lis r7, 0x21 806C8734. Originally lis r7, 0x10. Related to memory allocated for the entire CSS.
            #endif
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

        #ifdef MEMORY_EXPANSION
        // hook to start deallocating PACs when we leave SSS
        api->syInlineHookRel(0xD0BC,
                            reinterpret_cast<void*>(deallocatePacs),
                            Modules::SORA_SCENE);

        // hook to clear heaps before creating threads
        api->sySimpleHookRel(0xD264,
                            reinterpret_cast<void*>(__hookWrapper),
                            Modules::SORA_SCENE);
        #endif

        // hook to create threads when booting the CSS
        api->syInlineHookRel(0x3524, reinterpret_cast<void*>(createThreads), Modules::SORA_MENU_SEL_CHAR);

        // hook to load placeholder CSP if there is one
        api->sySimpleHookRel(0x14CE4, reinterpret_cast<void*>(__setPlaceholderTexture), Modules::SORA_MENU_SEL_CHAR);

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

        // // hook to load whether portrait is loaded or not
        // api->syInlineHookRel(0x00014D14,
        //                     reinterpret_cast<void*>(getPortraitLoaded),
        //                     Modules::SORA_MENU_SEL_CHAR);

        api->sySimpleHookRel(0x00014D08,
                            reinterpret_cast<void*>(skipPortraitChange),
                            Modules::SORA_MENU_SEL_CHAR);

        // hook to clear existing franchise icon behavior
        api->sySimpleHookRel(0x00014810,
                            reinterpret_cast<void*>(clearFranchiseIcons),
                            Modules::SORA_MENU_SEL_CHAR);

        #ifdef DEBUG
        // hook to log information about the portraits while they load
        api->sySimpleHookRel(0x00014CFC, reinterpret_cast<void*>(__logStuff), Modules::SORA_MENU_SEL_CHAR);
        #endif

    }
} // namespace CSSHooks
