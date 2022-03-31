#include <locale.h>
#include <regex>
#include <thread>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <chrono>
#include <signal.h>
#include <unistd.h>

#include "../include/pm_util.h"

enum
{
    DimmID,
    MediaReads,
    MediaWrites,
    ReadRequests,
    WriteRequests,
    TotalMediaReads,
    TotalMediaWrites,
    TotalReadRequests,
    TotalWriteRequests
};

// collect pmm dimm data
void util::PMMData::get_pmm_data()
{

    system("ipmctl show -performance > pmm_stat.txt");
    std::ifstream ipmctl_stat;
    ipmctl_stat.open("pmm_stat.txt"); // open the input file

    std::vector<std::string> reg_init_set = {
        R"(DimmID=0x([0-9a-f]*))",
        R"(^MediaReads=0x([0-9a-f]*))",
        R"(^MediaWrites=0x([0-9a-f]*))",
        R"(^ReadRequests=0x([0-9a-f]*))",
        R"(^WriteRequests=0x([0-9a-f]*))",
        R"(^TotalMediaReads=0x([0-9a-f]*))",
        R"(^TotalMediaWrites=0x([0-9a-f]*))",
        R"(^TotalReadRequests=0x([0-9a-f]*))",
        R"(^TotalWriteRequests=0x([0-9a-f]*))"};
    std::vector<std::regex> reg_set;
    std::regex stat_bit_convert_reg(R"(^([0-9a-f]{16})([0-9a-f]{16}))");
    for (auto i : reg_init_set)
    {
        reg_set.push_back(std::regex(i));
    }

    std::string str_line;
    std::smatch matched_data;
    std::smatch matched_num;
    int index;
    while (ipmctl_stat >> str_line)
    {
        for (int i = 0; i < reg_set.size(); i++)
        {
            if (std::regex_search(str_line, matched_data, reg_set.at(i)))
            {
                index = i;
                break;
            }
        }
        if (index == DimmID)
        {
            pmm_dimms_.push_back(DimmDataContainer());
            pmm_dimms_.back().dimm_id_ = matched_data[1];
        }
        else
        {
            uint64_t h64, l64;
            std::string str128b = matched_data[1];
            if (std::regex_search(str128b, matched_num, stat_bit_convert_reg))
            {
                pmm_dimms_.back().stat_[index - 1].h_u64b = std::stoull(matched_num[1], nullptr, 16);
                pmm_dimms_.back().stat_[index - 1].l_u64b = std::stoull(matched_num[2], nullptr, 16);
            }
            else
            {
                perror("parse dimm stat");
                exit(EXIT_FAILURE);
            }
        }
    }
}

util::PmmDataCollector::PmmDataCollector(const std::string name)
{
    start_timer = std::chrono::system_clock::now();
    start.get_pmm_data();
    outer_imc_read_addr_ = NULL;
    outer_imc_write_addr_ = NULL;
    outer_media_read_addr_ = NULL;
    outer_media_write_addr_ = NULL;
    dimm_info_ = NULL;
}

util::PmmDataCollector::PmmDataCollector(const std::string name, float *real_imc_read, float *real_imc_write)
{
    outer_imc_read_addr_ = real_imc_read;
    *outer_imc_read_addr_ = 0;
    outer_imc_write_addr_ = real_imc_write;
    *outer_imc_write_addr_ = 0;
    start_timer = std::chrono::system_clock::now();
    start.get_pmm_data();
    outer_media_read_addr_ = NULL;
    outer_media_write_addr_ = NULL;
    dimm_info_ = NULL;
}

util::PmmDataCollector::PmmDataCollector(const std::string name, float *real_imc_read, float *real_imc_write,
                                         float *real_media_read, float *real_media_write)
{
    outer_imc_read_addr_ = real_imc_read;
    *outer_imc_read_addr_ = 0;
    outer_imc_write_addr_ = real_imc_write;
    *outer_imc_write_addr_ = 0;
    outer_media_read_addr_ = real_media_read;
    *outer_media_read_addr_ = 0;
    outer_media_write_addr_ = real_media_write;
    *outer_media_write_addr_ = 0;
    start_timer = std::chrono::system_clock::now();
    start.get_pmm_data();
    dimm_info_ = NULL;
}

util::PmmDataCollector::PmmDataCollector(const std::string name, std::vector<util::DimmObj> *dimms)
{
    start_timer = std::chrono::system_clock::now();
    start.get_pmm_data();
    outer_imc_read_addr_ = NULL;
    outer_imc_write_addr_ = NULL;
    outer_media_read_addr_ = NULL;
    outer_media_write_addr_ = NULL;
    dimm_info_ = dimms;
};

// For details, please refer to "IPMWatch_Overview.pdf" mannual in VTune profiling kits by Intel.
// https://github.com/intel/intel-pmwatch/blob/master/docs/PMWatch_User_Guide.pdf

// read_64B_ops_received :
// write_64B_ops_received:
// Number of read and write operations performed to the physical media.
// Each operation transacts a 64 bytes operation. These operations includes
// commands transacted for maintenance as well as the commands transacted by the CPU.

// Formula:
// media_read_ops : (read_64B_ops_received - write_64B_ops_received) / 4
// media_write_ops: write_64B_ops_received / 4

// ddrt_read_ops :
// ddrt_write_ops:
// Number of read and write operations received from the CPU (memory controller),
// for the Memory Mode and App Direct Mode partitions.
util::PmmDataCollector::~PmmDataCollector()
{
    end.get_pmm_data();
    end_timer = std::chrono::system_clock::now();
    int dimm_num = end.pmm_dimms_.size();
    float media_read_size_MB = 0, imc_read_size_MB = 0, imc_write_size_MB = 0, media_write_size_MB = 0;

    setlocale(LC_NUMERIC, "");
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);
    if (print_flag_)
    {
        std::cerr << "-------------------------------------------------------------------------" << std::endl;
        fprintf(stderr, "elapsed time: %'.2f sec\n", duration.count() / 1000.0);
        std::cerr << "-------------------------------------------------------------------------" << std::endl;
        std::cerr << "|DIMM\t|RA\t|WA\t|iMC Rd(MB)\t|Media Rd(MB)\t|iMC Wr(MB)\t|Media Wr(MB)\t|" << std::endl;
    }
    if (dimm_info_)
    {
        dimm_info_->resize(dimm_num);
    }

    std::vector<float> TotalMediaReads_(dimm_num), TotalMediaWrites_(dimm_num), TotalReadRequests_(dimm_num), TotalWriteRequests_(dimm_num);
    for (int i = 0; i < dimm_num; i++)
    {
        for (int j = 0; j < 8; j++)
            assert(start.pmm_dimms_.at(i).stat_[j].h_u64b == end.pmm_dimms_.at(i).stat_[j].h_u64b);
        TotalMediaReads_.at(i) = (end.pmm_dimms_.at(i).stat_[TotalMediaReads - 1].l_u64b - start.pmm_dimms_.at(i).stat_[TotalMediaReads - 1].l_u64b) / 16384.0;
        TotalMediaWrites_.at(i) = (end.pmm_dimms_.at(i).stat_[TotalMediaWrites - 1].l_u64b - start.pmm_dimms_.at(i).stat_[TotalMediaWrites - 1].l_u64b) / 16384.0;
        TotalReadRequests_.at(i) = (end.pmm_dimms_.at(i).stat_[TotalReadRequests - 1].l_u64b - start.pmm_dimms_.at(i).stat_[TotalReadRequests - 1].l_u64b) / 16384.0;
        TotalWriteRequests_.at(i) = (end.pmm_dimms_.at(i).stat_[TotalWriteRequests - 1].l_u64b - start.pmm_dimms_.at(i).stat_[TotalWriteRequests - 1].l_u64b) / 16384.0;

        TotalMediaReads_.at(i) = TotalMediaReads_.at(i) - TotalMediaWrites_.at(i);
        if (dimm_info_)
        {
            dimm_info_->at(i).imc_read = TotalReadRequests_.at(i);
            dimm_info_->at(i).imc_wr = TotalWriteRequests_.at(i);
            dimm_info_->at(i).media_rd = TotalMediaReads_.at(i);
            dimm_info_->at(i).media_wr = TotalMediaWrites_.at(i);
            dimm_info_->at(i).dimm_id_ = "0x" + start.pmm_dimms_.at(i).dimm_id_;
        }
        imc_read_size_MB += TotalReadRequests_.at(i);
        media_read_size_MB += TotalMediaReads_.at(i);
        imc_write_size_MB += TotalWriteRequests_.at(i);
        media_write_size_MB += TotalMediaWrites_.at(i);
    }
    if (outer_imc_read_addr_)
        *outer_imc_read_addr_ = imc_read_size_MB;
    if (outer_imc_write_addr_)
        *outer_imc_write_addr_ = imc_write_size_MB;
    if (outer_media_read_addr_)
        *outer_media_read_addr_ = media_read_size_MB;
    if (outer_media_write_addr_)
        *outer_media_write_addr_ = media_write_size_MB;
    if (print_flag_)
    {
        for (int i = 0; i < dimm_num; i++)
        {
            std::cerr << "|0x" << start.pmm_dimms_.at(i).dimm_id_ << "\t";
            // printf("|%s\t", start.pmm_dimms_.at(i).dimm_id_);
            if ((TotalMediaReads_.at(i) / TotalReadRequests_.at(i)) > 5)
                fprintf(stderr, "|N/A\t");
            else
                fprintf(stderr, "|%'.2f\t", TotalMediaReads_.at(i) / TotalReadRequests_.at(i));
            if ((TotalMediaWrites_.at(i) / TotalWriteRequests_.at(i)) > 5)
                fprintf(stderr, "|N/A\t");
            else
                fprintf(stderr, "|%'.2f\t", TotalMediaWrites_.at(i) / TotalWriteRequests_.at(i));
            fprintf(stderr, "|%'8.2f\t", TotalReadRequests_.at(i));
            fprintf(stderr, "|%'8.2f\t", TotalMediaReads_.at(i));
            fprintf(stderr, "|%'8.2f\t", TotalWriteRequests_.at(i));
            fprintf(stderr, "|%'8.2f\t|\n", TotalMediaWrites_.at(i));
        }

        std::cerr << "\033[32m"
                  << "Total RA:" << media_read_size_MB / imc_read_size_MB << ", iMC read "
                  << imc_read_size_MB << "MB, media read " << media_read_size_MB << "MB"
                  << "\033[0m" << std::endl;
        std::cerr << "\033[31m"
                  << "Total WA:" << media_write_size_MB / imc_write_size_MB << ", iMC write "
                  << imc_write_size_MB << "MB, media write " << media_write_size_MB << "MB"
                  << "\033[0m" << std::endl;
    }
}

void util::PmmDataCollector::DisablePrint(void)
{
    print_flag_ = false;
}

// show progress
util::ProgressShow::ProgressShow(std::atomic<uint64_t> *progress, uint64_t total_amount)
{
    array_lock_.lock();
    progress_array_.push_back(progress);
    array_lock_.unlock();
    total_wss_ = total_amount;

    background_runner = std::thread{
        [&]()
        {
            while (1)
            {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                uint64_t progress = 0;
                array_lock_.lock();
                for (auto i : progress_array_)
                {
                    progress += (*i).load();
                }
                array_lock_.unlock();
                if ((progress * 100 / total_wss_) > 100)
                    break;
                OutPutSingleProgress(progress * 100 / total_wss_);
                if (stop_flag == 1)
                    break;
            }
        }};
}

util::ProgressShow::ProgressShow(uint64_t total_amount)
{
    total_wss_ = total_amount;

    background_runner = std::thread{
        [&]()
        {
            while (1)
            {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                uint64_t progress = 0;
                array_lock_.lock();
                for (auto i : progress_array_)
                {
                    progress += (*i).load();
                }
                array_lock_.unlock();
                if ((progress * 100 / total_wss_) > 100)
                    break;
                OutPutSingleProgress(progress * 100 / total_wss_);
                if (stop_flag == 1)
                    break;
            }
        }};
}

void util::ProgressShow::ProgressAppend(std::atomic<uint64_t> *progress)
{
    array_lock_.lock();
    progress_array_.push_back(progress);
    array_lock_.unlock();
}

util::ProgressShow::ProgressShow()
{
    // progress_ = nullptr;
    std::cout << "no proper input" << std::endl;
}
util::ProgressShow::~ProgressShow()
{
    stop_flag = 1;
    background_runner.join();
    std::cout << std::endl;
}

inline void util::ProgressShow::OutPutSingleProgress(int progress)
{
    assert(progress >= 0 && progress <= 100);
    std::cerr << std::flush << "\r[";
    for (int i = 0; i < progress; i++)
    {
        std::cerr << "=";
    }
    std::cerr << "=>";
    for (int i = 0; i < 100 - progress; i++)
    {
        std::cerr << " ";
    }

    std::cerr << "] ";
    std::cerr << progress << "%";
    std::cerr << std::flush;
}

void util::debug_perf_ppid(void)
{
    const pid_t ppid = getppid();
    char tmp[1024];
    sprintf(tmp, "/proc/%d/cmdline", ppid);
    FILE *const fc = fopen(tmp, "r");
    const size_t nr = fread(tmp, 1, 1020, fc);
    fclose(fc);
    // look for "perf record"
    if (nr < 11)
        return;
    tmp[nr] = '\0';
    for (size_t i = 0; i < nr; i++)
    {
        if (tmp[i] == 0)
            tmp[i] = ' ';
    }
    char *const perf = strstr(tmp, "perf record");
    if (perf == NULL)
        return;
    // it should be
    __perf_pid = ppid;
}

void util::debug_perf_switch(void)
{
    if (__perf_pid > 0)
        kill(__perf_pid, SIGUSR2);
}

void util::debug_perf_stop(void)
{
    if (__perf_pid > 0)
        kill(__perf_pid, SIGINT);
}