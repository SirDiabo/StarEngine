#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32) && !defined(_WIN64)
#define STAR_GLAPI __stdcall
#else
#define STAR_GLAPI
#endif

namespace ProfilerGpu {

struct Backend {
    void (STAR_GLAPI *genQueries)(int count, unsigned* ids) = nullptr;
    void (STAR_GLAPI *deleteQueries)(int count, const unsigned* ids) = nullptr;
    void (STAR_GLAPI *queryCounter)(unsigned id, unsigned target) = nullptr;
    void (STAR_GLAPI *getQueryObjectiv)(unsigned id, unsigned pname, int* out) = nullptr;
    void (STAR_GLAPI *getQueryObjectui64v)(unsigned id, unsigned pname, uint64_t* out) = nullptr;
};

constexpr unsigned kGlTimestamp = 0x8E28;
constexpr unsigned kGlQueryResult = 0x8866;
constexpr unsigned kGlQueryResultAvailable = 0x8867;

void SetBackend(const Backend* backend);
bool Available();
void Pump();
void FrameEnd();
uint32_t BeginScope(uint32_t nameId);
void EndScope(uint32_t handle);
size_t PendingFrameCount();
size_t FreeQueryCount();

}
