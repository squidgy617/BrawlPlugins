#include "sel_char_load_thread.h"
#include <OS/OSCache.h>
#include <os/OSError.h>
#include <VI/vi.h>
#include <nw4r/g3d/g3d_resfile.h>
#include <gf/gf_heap_manager.h>
#include <gf/gf_file_io_manager.h>
#include <memory.h>
#include <mu/menu.h>
#include <cstdio>

#define MEMORY_EXPANSION
// #define DEBUG

#ifdef MEMORY_EXPANSION
Heaps::HeapType threadBufferHeap = Heaps::Fighter3Resource;
Heaps::HeapType inactiveBufferHeap = Heaps::Fighter4Resource;
u32 threadBufferSize = 0xE0000;
#else
Heaps::HeapType threadBufferHeap = Heaps::MenuResource;
Heaps::HeapType inactiveBufferHeap = Heaps::MenuResource;
u32 threadBufferSize = 0x40000;
#endif

const u8 excludedSelchKinds[] = {0x29, 0x38, 0x39, 0x3A, 0x3B};
const u8 excludedSelchKindLength = sizeof(excludedSelchKinds) / sizeof(excludedSelchKinds[0]);
const u8 noLoadSelchKinds[] = {0x28, 0x29};
const u8 noLoadSelchKindLength = sizeof(noLoadSelchKinds) / sizeof(noLoadSelchKinds[0]);

selCharLoadThread* selCharLoadThread::s_threads[muSelCharTask::num_player_areas] = {};

selCharLoadThread::selCharLoadThread(muSelCharPlayerArea* area)
{
    m_loaded = -1;
    m_toLoad = -1;
    m_playerArea = area;
    m_dataReady = false;
    m_isRunning = false;
    m_shouldUpdateEmblem = false;
    m_shouldUpdateName = false;
    m_lastSelectedCharKind = -1;
    m_activeBuffer = 0;
    m_inactiveBuffer = 1;
    m_readyToDisplay = false;
    m_skipDelay = false;
    m_shouldUpdateMaterial = false;
    m_actuallyUpdateMaterial = false;

    m_fileBuffer = gfHeapManager::alloc(threadBufferHeap, threadBufferSize);
    m_buffers[m_activeBuffer] = area->m_charPicData;
    m_buffers[m_inactiveBuffer] = gfHeapManager::alloc(inactiveBufferHeap, threadBufferSize);
    s_threads[area->m_areaIdx] = this;
}

bool selCharLoadThread::findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind) {
    for (u8 i = 0; i < muSelCharTask::num_player_areas; i++) {
        if (this->getAreaIdx() != i) {
            if (s_threads[i] != nullptr &&
                s_threads[i]->isTargetPortraitReady(selchKind)) {
                this->setData(s_threads[i]->getFileBuffer());
                return true;
            }
        }
    }
    return false;
}

void selCharLoadThread::main()
{
    muSelCharPlayerArea* area = this->m_playerArea;
    const char* format = "";
    // TODO: instead of hardcoded IDs, always check for alt archive first, then RSP archive?
    if (isExcludedSelchKind(m_toLoad)) {
        format = "/menu/common/char_bust_alt/MenSelchrFaceB%02d0.brres";
    }
    else {
        format = "/menu/common/char_bust_tex/MenSelchrFaceB%02d0.brres";
    }
    char filepath[0x34];

    // Data is finished loading
    if (this->m_isRunning && this->m_handle.isReady() && this->m_handle.getReturnStatus() == 0)
    {
        this->m_isRunning = false;

        if (!isNoLoadSelchKind(area->m_charKind) && this->m_toLoad == -1)
        {
            this->m_dataReady = true;
            this->m_skipDelay = true;
            area->setCharPic(this->m_loaded,
                             area->m_playerKind,
                             area->m_charColorNo,
                             area->isTeamBattle(),
                             area->m_teamColor,
                             area->m_teamSet);
        }
        else if (isNoLoadSelchKind(area->m_charKind)) {
            this->m_dataReady = true;
            this->m_skipDelay = true;
            area->setCharPic(area->m_charKind,
                             area->m_playerKind,
                             area->m_charColorNo,
                             area->isTeamBattle(),
                             area->m_teamColor,
                             area->m_teamSet);
        }
    }
    else if ((this->m_isRunning && this->m_handle.isReady() && this->m_handle.getReturnStatus() == 1)) {
        OSReport("Could not load RSP archive for slot %d\n", area->m_charKind);
        this->m_handle.cancelRequest();
        this->m_dataReady = false;
        this->m_isRunning = false;
    }

    if (this->m_toLoad != -1)
    {
        if (!this->m_isRunning)
        {
            this->m_loaded = this->m_toLoad;
            this->m_toLoad = -1;
            if (this->findAndCopyThreadWithPortraitAlreadyLoaded(this->m_loaded)) {
                this->m_dataReady = true;
                this->m_skipDelay = true;
                area->setCharPic(this->m_loaded,
                                 area->m_playerKind,
                                 area->m_charColorNo,
                                 area->isTeamBattle(),
                                 area->m_teamColor,
                                 area->m_teamSet);
            }
            else {
                // Clear read request and signal that read is in progress

                this->m_isRunning = true;
                this->m_dataReady = false;

                // Handles conversions for poketrio and special slots
                int charKind = this->m_loaded;
                int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
                id = muMenu::getStockFrameID(id);
                sprintf(filepath, format, id);

                // Start the read process
                this->m_handle.readRequest(filepath, this->getFileBuffer(), 0, 0);
            }


        }
    }

    // If thread is ready to display image (after 1 frame delay), init resfile
    if (this->isReadyToDisplay())
    {
        // init resFile
        nw4r::g3d::ResFile::Init(&area->m_charPicRes);

        // Test code for changing material directly
        // char name[64];
        // int charKind = this->m_loaded;
        // int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
        // id = muMenu::getStockFrameID(id);
        // sprintf(name, "MenSelchrFaceB.%03d", area->m_charColorNo + 1 + (id * 10));
        // OSReport("%s %d %d \n", name, area->m_charColorNo, area->m_charKind);
        // nw4r::g3d::ResFile* resFile = &area->m_charPicRes;
        // area->m_muCharPic->changeMaterialTex(1, name, &resFile);
        // area->m_muCharPic->changeMaterialTex(0, name, &resFile);

        // this->materialUpdated();

        // Mark image as displayed
        this->imageDisplayed();
    }
    else if (this->m_shouldUpdateMaterial)
    {
        this->m_actuallyUpdateMaterial = true;
        area->setCharPic(area->m_charKind,
                             area->m_playerKind,
                             area->m_charColorNo,
                             area->isTeamBattle(),
                             area->m_teamColor,
                             area->m_teamSet);
    }
}


void selCharLoadThread::requestLoad(int charKind, bool hasCsp)
{
    if (isNoLoadSelchKind(charKind)) {
        m_toLoad = -1;
    }
    else {
        m_toLoad = charKind;
        if (!hasCsp) {
            m_shouldUpdateEmblem = false;
            m_shouldUpdateName = false;
            m_shouldUpdateMaterial = false;
            m_actuallyUpdateMaterial = false;
        }
    }
    m_lastSelectedCharKind = charKind;
    m_dataReady = false;
}

void selCharLoadThread::reset()
{
//    if (m_isRunning)
//    {
//        m_handle.cancelRequest();
//        m_isRunning = false;
//    }

    m_dataReady = false;
    m_shouldUpdateEmblem = false;
    m_shouldUpdateName = false;
    m_shouldUpdateMaterial = false;
    m_actuallyUpdateMaterial = false;
    m_toLoad = -1;
}

bool selCharLoadThread::isTargetPortraitReady(u8 selchKind) {
    return !(!this->isReady() || this->isRunning() || this->getToLoadCharKind() != -1 || selchKind != this->getLoadedCharKind() || selchKind != this->m_playerArea->m_charKind);
}

void selCharLoadThread::setData(void* copy) {
    memcpy(this->getFileBuffer(), copy, threadBufferSize);
}

selCharLoadThread::~selCharLoadThread()
{
    free(m_fileBuffer);
    free(m_buffers[m_activeBuffer]);
    free(m_buffers[m_inactiveBuffer]);
    m_handle.release();
    s_threads[this->getAreaIdx()] = NULL;
}

bool selCharLoadThread::isExcludedSelchKind(u8 selchKind) {
    for (u8 i = 0; i < excludedSelchKindLength; i++) {
        if (excludedSelchKinds[i] == selchKind) {
            return true;
        }
    }
    return false;
}

bool selCharLoadThread::isNoLoadSelchKind(u8 selchKind) {
    for (u8 i = 0; i < noLoadSelchKindLength; i++) {
        if (noLoadSelchKinds[i] == selchKind) {
            return true;
        }
    }
    return false;
}


selCharLoadThread* selCharLoadThread::getThread(u8 areaIdx) {
    return s_threads[areaIdx];
}
