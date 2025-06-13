#pragma once

#include <OS/OSMutex.h>
#include <OS/OSThread.h>
#include <gf/gf_file_io_handle.h>
#include <mu/selchar/mu_selchar_player_area.h>
#include <types.h>

class selCharLoadThread : public OSThread {
protected:
    static selCharLoadThread* s_threads[muSelCharTask::num_player_areas];

    gfFileIOHandle m_handle;
    muSelCharPlayerArea* m_playerArea;
    void* m_buffer;
    int m_toLoad;
    int m_loaded;
    bool m_dataReady;
    bool m_isRunning;

public:
    selCharLoadThread(muSelCharPlayerArea* area);
    void requestLoad(int charKind);
    void main();
    void start();
    void suspend();
    void resume();
    void reset();
    void* getBuffer() { return m_buffer; }
    bool isRunning() { return m_isRunning; }
    bool isReady() { return m_dataReady; }
    void setCharPic();
    int getAreaIdx() { return m_playerArea->m_areaIdx; }
    int getToLoadCharKind() { return m_toLoad; }
    int getLoadedCharKind() { return m_loaded; }
    bool isTargetPortraitReady(u8 selchKind);
    bool findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind);
    void setData(void* m_copy);

    static bool isExcludedSelchKind(u8 selchKind);
    static selCharLoadThread* getThread(u8 areaIdx);

    ~selCharLoadThread();
};




