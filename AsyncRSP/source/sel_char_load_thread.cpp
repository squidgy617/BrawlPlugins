#include "sel_char_load_thread.h"
#include <OS/OSCache.h>
#include <VI/vi.h>
#include <gf/gf_heap_manager.h>
#include <gf/gf_file_io_manager.h>
#include <memory.h>
#include <mu/menu.h>
#include <cstdio>

const u8 excludedSelchKinds[] = {0x29, 0x38, 0x39, 0x3A, 0x3B};
const u8 excludedSelchKindLength = sizeof(excludedSelchKinds) / sizeof(excludedSelchKinds[0]);

selCharLoadThread* selCharLoadThread::s_threads[muSelCharTask::num_player_areas] = {};

selCharLoadThread::selCharLoadThread(muSelCharPlayerArea* area)
{
    m_loaded = -1;
    m_toLoad = -1;
    m_playerArea = area;
    m_dataReady = false;
    m_isRunning = false;

    m_buffer = gfHeapManager::alloc(Heaps::MenuResource, 0x40000);
    s_threads[area->m_areaIdx] = this;
}

bool selCharLoadThread::findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind) {
    for (u8 i = 0; i < muSelCharTask::num_player_areas; i++) {
        if (this->getAreaIdx() != i) {
            if (s_threads[i]->isTargetPortraitReady(selchKind)) {
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
    const char* format = "/menu/common/char_bust_tex/MenSelchrFaceB%02d0.brres";
    char filepath[0x34];

    // Data is finished loading
    if (this->m_isRunning && this->m_handle.isReady())
    {
        this->m_isRunning = false;

        if (this->m_toLoad == -1)
        {
            this->m_dataReady = true;
            area->setCharPic(this->m_loaded,
                             area->m_playerKind,
                             area->m_charColorNo,
                             area->isTeamBattle(),
                             area->m_teamColor,
                             area->m_teamSet);
        }
    }

    if (this->m_toLoad != -1)
    {
        if (!this->m_isRunning)
        {
            this->m_loaded = this->m_toLoad;
            this->m_toLoad = -1;
            if (this->findAndCopyThreadWithPortraitAlreadyLoaded(this->m_loaded)) {
                this->m_dataReady = true;
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
                this->m_handle.readRequest(filepath, this->m_buffer, 0, 0);
            }


        }
    }

    // if (this->isTargetPortraitReady(area->m_charKind))
    // {
    //     area->dispMarkKind(area->m_charKind);
    // }
}

// TODO: Load emblem when pic is loaded


void selCharLoadThread::requestLoad(int charKind)
{
    if (isExcludedSelchKind(charKind)) {
        m_toLoad = -1;
    }
    else {
        m_toLoad = charKind;
    }
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
    m_toLoad = -1;
}

bool selCharLoadThread::isTargetPortraitReady(u8 selchKind) {
    return !(!this->isReady() || this->isRunning() || this->getToLoadCharKind() != -1 || selchKind != this->getLoadedCharKind() || selchKind != this->m_playerArea->m_charKind);
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


selCharLoadThread* selCharLoadThread::getThread(u8 areaIdx) {
    return s_threads[areaIdx];
}
