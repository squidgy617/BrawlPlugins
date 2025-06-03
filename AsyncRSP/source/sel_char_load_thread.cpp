#include "sel_char_load_thread.h"
#include <OS/OSCache.h>
#include <VI/vi.h>
#include <gf/gf_heap_manager.h>
#include <gf/gf_file_io_manager.h>
#include <memory.h>
#include <mu/menu.h>
#include <cstdio>

selCharLoadThread::selCharLoadThread(muSelCharPlayerArea* area)
{
    m_loaded = -1;
    m_toLoad = -1;
    m_playerArea = area;
    m_dataReady = false;
    m_isRunning = false;

    m_buffer = gfHeapManager::alloc(Heaps::MenuResource, 0x40000);
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
            int charKind = this->m_toLoad;

            // If read is already in progress, cancel it and start new read request
            //                if (thread->m_isRunning)
            //                {
            //                    thread->reset();
            //                }

            // Handles conversions for poketrio and special slots
            int id = muMenu::exchangeMuSelchkind2MuStockchkind(charKind);
            id = muMenu::getStockFrameID(id);

            // Clear read request and signal that read is in progress
            this->m_isRunning = true;
            this->m_dataReady = false;
            this->m_loaded = this->m_toLoad;
            this->m_toLoad = -1;

            sprintf(filepath, format, id);

            // Start the read process
            this->m_handle.readRequest(filepath, this->m_buffer, 0, 0);
        }
    }
}


void selCharLoadThread::requestLoad(int charKind)
{
    m_toLoad = charKind;
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

selCharLoadThread::~selCharLoadThread()
{
    free(m_buffer);
    m_handle.release();
}

// TODO: Fix race condition
// TODO: Copy buffer if other players already loaded that character?