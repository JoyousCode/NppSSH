#pragma once
//#include "SSHWindow.h"
#include "SSHBasePanel.h"

class SSHConEmu : public SSHBasePanel{
public:
    SSHConEmu(int panelSeqId, int panelrealId);
    ~SSHConEmu() override;
    void initPanel();
    INT_PTR CALLBACK run_dlgProc(UINT message, WPARAM wParam, LPARAM lParam) override;
private:

};

// NPP启动重建面板具体实现
void SSHConEmu_InitRecreatePanel(SSHConEmu* pNewPanel);