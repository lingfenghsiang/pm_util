#pragma once
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

namespace util
{
    class DimmObj{
        public:
        std::string dimm_id_;
        float imc_read, imc_wr, media_rd, media_wr;
    };

    class DimmAttribute128b
    {
    public:
        uint64_t l_u64b, h_u64b;
    };

    class DimmDataContainer
    {
    public:
        std::string dimm_id_;
        DimmAttribute128b stat_[8];
    };

    class PMMData
    {
    public:
        std::vector<DimmDataContainer> pmm_dimms_;
        void get_pmm_data();

    };

    class PmmDataCollector{
        public:
            PmmDataCollector(const std::string name);
            PmmDataCollector(const std::string name, float *real_imc_read, float *real_imc_write);
            PmmDataCollector(const std::string name, float *real_imc_read, float *real_imc_write,
                             float *real_media_read, float *real_media_write);
            PmmDataCollector(const std::string name, std::vector<DimmObj> *dimms);
            ~PmmDataCollector();
            void DisablePrint(void);

        private:
            PMMData start, end;
            bool print_flag_ = true;

            std::chrono::_V2::system_clock::time_point start_timer, end_timer;
            float *outer_imc_read_addr_ = nullptr, *outer_imc_write_addr_ = nullptr,
                  *outer_media_read_addr_ = nullptr, *outer_media_write_addr_ = nullptr;
            std::vector<DimmObj> *dimm_info_;
    };

    void ShowReadAmp(PMMData start, PMMData end);

    void ShowWriteAmp(PMMData start, PMMData end);

    class ProgressShow
    {
    public:
        std::vector<std::atomic<uint64_t> *> progress_array_;
        uint64_t total_wss_;
        std::thread background_runner;
        std::mutex array_lock_;
        int stop_flag = 0;

        ProgressShow(std::atomic<uint64_t> *progress, uint64_t total_amount);
        ProgressShow(uint64_t total_amount);
        ProgressShow();
        ~ProgressShow();
        inline void OutPutSingleProgress(int progress);
        void ProgressAppend(std::atomic<uint64_t> *progress);
    };

    static pid_t __perf_pid = 0;
    void debug_perf_ppid(void);
    void debug_perf_switch(void);
    void debug_perf_stop(void);

} // namespace util