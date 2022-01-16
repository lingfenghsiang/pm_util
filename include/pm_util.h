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

        private:
            PMMData *start, *end;

            std::chrono::_V2::system_clock::time_point start_timer, end_timer;
            float *outer_imc_read_addr_ = nullptr, *outer_imc_write_addr_ = nullptr,
                  *outer_media_read_addr_ = nullptr, *outer_media_write_addr_ = nullptr;
            std::vector<DimmObj> *dimm_info_;
    };

    void ShowReadAmp(PMMData start, PMMData end);

    void ShowWriteAmp(PMMData start, PMMData end);

} // namespace util