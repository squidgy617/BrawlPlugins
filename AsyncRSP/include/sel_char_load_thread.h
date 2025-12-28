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
    void * m_buffers[2];
    int m_inactiveBuffer;
    int m_activeBuffer;
    void* m_fileBuffer;
    int m_toLoad;
    int m_loaded;
    bool m_readyToDisplay;
    bool m_dataReady;
    bool m_isRunning;
    bool m_shouldUpdateEmblem;
    bool m_shouldUpdateName;
    int m_lastSelectedCharKind;

public:
    selCharLoadThread(muSelCharPlayerArea* area);
    void requestLoad(int charKind, bool hasCsp = false);
    void main();
    void start();
    void suspend();
    void resume();
    void reset();
    void* getInactiveBuffer() { return m_buffers[m_inactiveBuffer]; }
    void* getActiveBuffer() { return m_buffers[m_activeBuffer]; }
    void* getFileBuffer() { return m_fileBuffer; }
    bool isRunning() { return m_isRunning; }
    bool isReady() { return m_dataReady; }
    void imageDisplayed() { m_readyToDisplay = false; }
    bool isReadyToDisplay() { return m_readyToDisplay; }
    void setCharPic();
    int getAreaIdx() { return m_playerArea->m_areaIdx; }
    int getToLoadCharKind() { return m_toLoad; }
    int getLoadedCharKind() { return m_loaded; }
    bool isTargetPortraitReady(u8 selchKind);
    bool findAndCopyThreadWithPortraitAlreadyLoaded(u8 selchKind);
    void setData(void* m_copy);
    void setFrameTex(u8 areaIdx, u8 frameIndex);
    void imageLoaded() { m_shouldUpdateEmblem = true; m_shouldUpdateName = true; m_readyToDisplay = true; }
    bool shouldUpdateEmblem() { return m_shouldUpdateEmblem; }
    bool shouldUpdateName() { return m_shouldUpdateName; }
    void emblemUpdated() { m_shouldUpdateEmblem = false; }
    void nameUpdated() { m_shouldUpdateName = false; }
    void swapBuffers() { int holdBuffer = m_activeBuffer; m_activeBuffer = m_inactiveBuffer; m_inactiveBuffer = holdBuffer; }

    static bool isExcludedSelchKind(u8 selchKind);
    static bool isNoLoadSelchKind(u8 selchKind);
    static selCharLoadThread* getThread(u8 areaIdx);

    ~selCharLoadThread();
};




