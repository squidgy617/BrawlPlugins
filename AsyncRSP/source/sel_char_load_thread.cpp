#include "sel_char_load_thread.h"
#include <OS/OSCache.h>
#include <os/OSError.h>
#include <VI/vi.h>
#include <gf/gf_heap_manager.h>
#include <gf/gf_file_io_manager.h>
#include <memory.h>
#include <mu/menu.h>
#include <cstdio>

const u8 excludedSelchKinds[] = {0x29, 0x38, 0x39, 0x3A, 0x3B};
const u8 excludedSelchKindLength = sizeof(excludedSelchKinds) / sizeof(excludedSelchKinds[0]);
const u8 noLoadSelchKinds[] = {0x28, 0x29};
const u8 noLoadSelchKindLength = sizeof(noLoadSelchKinds) / sizeof(noLoadSelchKinds[0]);

selCharLoadThread* selCharLoadThread::s_threads[muSelCharTask::num_player_areas] = {};

selCharLoadThread::selCharLoadThread(muSelCharPlayerArea* area)
{
    m_loaded = -1;
    m_toLoad = -1;
    m_toLoadCosNo = -1;
    m_loadedCosNo = -1;
    m_playerArea = area;
    m_dataReady = false;
    m_isRunning = false;
    m_updateEmblem = false;
    m_updateName = false;
    m_lastSelectedCharKind = -1;

    m_buffer = gfHeapManager::alloc(Heaps::MenuResource, 0x40000);
    s_threads[area->m_areaIdx] = this;
}

bool selCharLoadThread::findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind, u8 selchNo = 0) {
    for (u8 i = 0; i < muSelCharTask::num_player_areas; i++) {
        if (this->getAreaIdx() != i) {
            if (s_threads[i]->isTargetPortraitReady(selchKind, selchNo)) {
                this->setData(s_threads[i]->getBuffer());
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
        format = "/menu/common/char_bust_alt/MenSelchrFaceB%02d%d.brres";
    }
    else {
        format = "/menu/common/char_bust_tex/MenSelchrFaceB%02d%d.brres";
    }
    char filepath[0x34];

    // Data is finished loading
    if (this->m_isRunning && this->m_handle.isReady() && this->m_handle.getReturnStatus() == 0)
    {
        this->m_isRunning = false;

        if (!isNoLoadSelchKind(area->m_charKind) && this->m_toLoad == -1)
        {
            this->m_dataReady = true;
            this->imageLoaded();
            area->setCharPic(this->m_loaded,
                             area->m_playerKind,
                             area->m_charColorNo,
                             area->isTeamBattle(),
                             area->m_teamColor,
                             area->m_teamSet);
        }
        else if (isNoLoadSelchKind(area->m_charKind)) {
            this->m_dataReady = true;
            imageLoaded();
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
            this->m_loadedCosNo = this->m_toLoadCosNo;
            this->m_toLoadCosNo = -1;
            if (this->findAndCopyThreadWithPortraitAlreadyLoaded(this->m_loaded,this->m_loadedCosNo)) {
                this->m_dataReady = true;
                this->imageLoaded();
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
                int id_cosNum = this->m_loadedCosNo/10;
                sprintf(filepath, format, id, id_cosNum);

                // Start the read process
                this->m_handle.readRequest(filepath, this->m_buffer, 0, 0);
            }


        }
    }
}


void selCharLoadThread::requestLoad(int charKind, u8 selchNo)
{
    if (isNoLoadSelchKind(charKind)) {
        m_toLoad = -1;
        m_toLoadCosNo = -1;
    }
    else {
        m_toLoad = charKind;
        m_toLoadCosNo = muMenu::getFighterColorFileNo(charKind,selchNo);
        m_updateEmblem = false;
        m_updateName = false;
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
    m_updateEmblem = false;
    m_updateName = false;
    m_toLoad = -1;
    m_toLoadCosNo = -1;
}

bool selCharLoadThread::isTargetPortraitReady(u8 selchKind, u8 selchNo) {
    return !(!this->isReady() || this->isRunning() || this->getToLoadCharKind() != -1 || selchKind != this->getLoadedCharKind() || selchKind != this->m_playerArea->m_charKind || this->m_toLoadCosNo != -1 || (selchKind == this->getLoadedCharKind() && selchNo/10 != this->getLoadedCosNo()/10));
}

void selCharLoadThread::setData(void* copy) {
    memcpy(this->m_buffer, copy, 0x40000);
}

selCharLoadThread::~selCharLoadThread()
{
    free(m_buffer);
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
