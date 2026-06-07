// First-party no-op stub: the NSF engine never enables CPU logging (SetLogger(nullptr)).
// This avoids pulling nsf.h / fileutil.h into the build.
#include "log_cpu.h"

namespace xgm
{

CPULogger::CPULogger()
    : log_level(0), soundchip(0), file(nullptr), filename(nullptr),
      frame_count(0), cpu(nullptr), nsf(nullptr)
{
    bank[0] = bank[1] = bank[2] = bank[3] =
    bank[4] = bank[5] = bank[6] = bank[7] = 0;
}

CPULogger::~CPULogger()
{
}

void CPULogger::Reset()
{
}

bool CPULogger::Write(UINT32 /*adr*/, UINT32 /*val*/, UINT32 /*id*/)
{
    return false;
}

bool CPULogger::Read(UINT32 /*adr*/, UINT32& val, UINT32 /*id*/)
{
    val = 0;
    return false;
}

void CPULogger::SetOption(int /*id*/, int /*val*/)
{
}

int CPULogger::GetLogLevel() const
{
    return 0;
}

void CPULogger::SetSoundchip(UINT8 /*soundchip_*/)
{
}

void CPULogger::SetFilename(const char* /*filename_*/)
{
}

void CPULogger::Begin(const char* /*title*/)
{
}

void CPULogger::Init(UINT8 /*reg_a*/, UINT8 /*reg_x*/)
{
}

void CPULogger::Play()
{
}

void CPULogger::SetCPU(NES_CPU* /*c*/)
{
}

void CPULogger::SetNSF(NSF* /*n*/)
{
}

} // namespace xgm
