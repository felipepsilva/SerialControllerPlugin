#pragma once

#include "IPlug_include_in_plug_hdr.h"

const int kNumPresets = 1;

enum EParams {
    kParamGain = 0,
    kNumParams
};

enum EControlTags {
    kNumCtrlTags
};

using namespace iplug;
using namespace igraphics;

class SerialController final : public Plugin {
private:
    HANDLE m_Comm;

    bool m_SerialWaiting = false;
    OVERLAPPED m_OverlappedReading = { 0 };

    void InitSerial();
    void ReadSerial();

public:
    SerialController(const InstanceInfo& info);

    void OnIdle() override;
    void OnUIOpen() override;

#if IPLUG_DSP // http://bit.ly/2S64BDd
public:
    void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
    void ProcessMidiMsg(const IMidiMsg& msg) override;
#endif
};
