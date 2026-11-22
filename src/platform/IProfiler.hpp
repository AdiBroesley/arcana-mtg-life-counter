#pragma once
#include <chrono>
#include <cstdint>

namespace mtg {
enum class Work { HistoryRecord, UndoApply, UndoLabel };
class IProfiler {
public:
 virtual ~IProfiler()=default;
 virtual bool recording()const=0;
 virtual void record(Work work,std::uint64_t micros)=0;
};

// Inactive probes do not read the clock or allocate memory.
class ScopedWork {
public:
 ScopedWork(IProfiler* profiler,Work work):profiler_(profiler&&profiler->recording()?profiler:nullptr),work_(work){
  if(profiler_)started_=std::chrono::steady_clock::now();
 }
 ~ScopedWork(){if(profiler_)profiler_->record(work_,std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-started_).count());}
 ScopedWork(const ScopedWork&)=delete;
 ScopedWork& operator=(const ScopedWork&)=delete;
private:
 IProfiler* profiler_;Work work_;std::chrono::steady_clock::time_point started_{};
};
}
