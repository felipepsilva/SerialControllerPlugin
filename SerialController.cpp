#include "SerialController.h"
#include "IControls.h"
#include "IPlug_include_in_plug_src.h"

SerialController::SerialController(const InstanceInfo& info)
    : Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
    GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100.0, 0.01, "%");
#if IPLUG_DSP
    SetTailSize(4410000);
#endif

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
    mMakeGraphicsFunc = [&]() {
        return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_HEIGHT));
    };

    mLayoutFunc = [&](IGraphics* pGraphics) {
        auto actionFunc = [&](IControl* pCaller) {
            static bool onoff = false;
            onoff = !onoff;
            IMidiMsg msg;
            constexpr int pitches[3] = { 60, 65, 67 };

            for (int i = 0; i < 3; i++) {
                if (onoff)
                    msg.MakeNoteOnMsg(pitches[i], 60, 0);
                else
                    msg.MakeNoteOffMsg(pitches[i], 0);

                SendMidiMsgFromUI(msg);
            }

            SplashClickActionFunc(pCaller);
        };

        pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
        pGraphics->AttachPanelBackground(COLOR_GRAY);
        pGraphics->AttachControl(new IVButtonControl(pGraphics->GetBounds().GetPadded(-10), actionFunc, "Trigger Chord"));
    };
#endif
}

#if IPLUG_DSP
void SerialController::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
    const double gain = GetParam(kParamGain)->Value() / 100.;
    const int nChans = NOutChansConnected();

    for (auto s = 0; s < nFrames; s++) {
        for (auto c = 0; c < nChans; c++) {
            outputs[c][s] = outputs[c][s] * gain;
        }
    }
}

void SerialController::ProcessMidiMsg(const IMidiMsg& msg)
{
    TRACE;

    int status = msg.StatusMsg();

    switch (status) {
    case IMidiMsg::kNoteOn:
    case IMidiMsg::kNoteOff:
    case IMidiMsg::kPolyAftertouch:
    case IMidiMsg::kControlChange:
    case IMidiMsg::kProgramChange:
    case IMidiMsg::kChannelAftertouch:
    case IMidiMsg::kPitchWheel: {
        goto handle;
    }
    default:
        return;
    }

handle:
    SendMidiMsg(msg);
}
#endif

void SerialController::OnUIOpen()
{
    InitSerial();
}

void SerialController::OnIdle()
{
    ReadSerial();
}

void SerialController::InitSerial()
{
    m_Comm = CreateFileA(
        "COM8",
        GENERIC_READ,
        0,
        0,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        0);

    if (m_Comm == INVALID_HANDLE_VALUE) {
        MessageBoxA(NULL, "Couldn't open port!", "ERROR", MB_ICONERROR | MB_OK);
        return;
    }

    DCB dcb = { 0 };
    dcb.DCBlength = sizeof(DCB);
    if (GetCommState(m_Comm, &dcb) == false) {
        MessageBoxA(NULL, "Couldn't get port state!", "ERROR", MB_ICONERROR | MB_OK);
        return;
    }

    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.Parity = 0;
    dcb.StopBits = ONESTOPBIT;

    if (SetCommState(m_Comm, &dcb) == false) {
        MessageBoxA(NULL, "Couldn't set port state!", "ERROR", MB_ICONERROR | MB_OK);
        return;
    }

    OutputDebugString("\nSERIAL PORT INITIALIZED!!!\n");
}

void SerialController::ReadSerial()
{
    if (m_Comm != INVALID_HANDLE_VALUE) {
        char buffer[1];
        DWORD bytesRead;

        auto handleReading = [&]() {
            IMidiMsg msg;
            if (bytesRead == 1) {
                msg.MakeControlChangeMsg(IMidiMsg::EControlChangeMsg(16 + buffer[0]), 1, 0, 0);
                SendMidiMsg(msg);
            }
        };

        if (m_OverlappedReading.hEvent == NULL) {
            m_OverlappedReading.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        }

        if (m_OverlappedReading.hEvent == NULL) {
            OutputDebugString("Failed to create overlapped event!");
            return;
        }

        if (m_SerialWaiting) {
            DWORD res = WaitForSingleObject(m_OverlappedReading.hEvent, 35); // 35 ms timeout
            switch (res) {
            case WAIT_OBJECT_0:
                if (!GetOverlappedResult(m_Comm, &m_OverlappedReading, &bytesRead, FALSE)) {
                    OutputDebugString("Failed to get overlapped result!");
                } else {
                    handleReading();
                }
                m_SerialWaiting = false;

                break;
            case WAIT_TIMEOUT:
                // operation isn't complete yet
                break;
            default:
                break;
            }
        } else {
            if (!ReadFile(m_Comm, buffer, 1, &bytesRead, &m_OverlappedReading)) {
                if (GetLastError() != ERROR_IO_PENDING) {
                    OutputDebugString("Error reading from port!");
                } else {
                    m_SerialWaiting = true;
                }
            } else {
                handleReading();
            }
        }
    }
}