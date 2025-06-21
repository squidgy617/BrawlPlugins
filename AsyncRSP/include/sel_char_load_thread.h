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
    int m_toLoad, m_toLoadCosNo;
    int m_loaded, m_loadedCosNo;
    bool m_dataReady;
    bool m_isRunning;
    bool m_updateEmblem;
    bool m_updateName;
    int m_lastSelectedCharKind;

public:
    selCharLoadThread(muSelCharPlayerArea* area);
    void requestLoad(int charKind, u8 selchNo);
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
    int getLoadedCosNo() { return m_loadedCosNo; }
    void setLoadedCosNo(int cosNo) { m_loadedCosNo = cosNo; }
    bool isTargetPortraitReady(u8 selchKind, u8 selchNo);
    bool findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind, u8 selchNo);
    void setData(void* m_copy);
    void setFrameTex(u8 areaIdx, u8 frameIndex);
    void imageLoaded() { m_updateEmblem = true; m_updateName = true; }
    bool updateEmblem() { return m_updateEmblem; }
    bool updateName() { return m_updateName; }
    void emblemUpdated() { m_updateEmblem = false; }
    void nameUpdated() { m_updateName = false; }

    static bool isExcludedSelchKind(u8 selchKind);
    static bool isNoLoadSelchKind(u8 selchKind);
    static selCharLoadThread* getThread(u8 areaIdx);

    ~selCharLoadThread();
};




