#include <iostream>
#include <vector>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <bitset>
#include <iostream>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/kernel-page-flags.h>
#include <stdint.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <map>
#include <list>
#include <utility>
#include <math.h>
#include <fstream>
#include <x86intrin.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <random>
#include <set>
#include <sys/syscall.h>
#include "solver.h"
#include "sysinfo.h"


using namespace std;


#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)
#define PAGE_SHIFT 12
#define PAGEMAP_LENGTH 8

static int g_pagemap_fd = -1;
const uint64_t SIZE_GB = 20;
const uint64_t SIZE = SIZE_GB * 1024 * 1024 * 1024ULL;
const uint64_t NUM_ACCESS = 2000;

typedef std::map<size_t,std::vector<uint64_t>> AddressSet;


void* tryAllocate1Gb()
{
    auto space = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE,
                  MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_1GB
                    , -1, 0);
    return space;
}

void* tryAllocate2Mb()
{
    auto space = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE,
                  MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB | MAP_HUGE_2MB
                    , -1, 0);
    return space;
}

void* tryAllocate4Kb()
{
    auto space = mmap(nullptr, SIZE, PROT_READ | PROT_WRITE,
                  MAP_POPULATE | MAP_ANONYMOUS | MAP_PRIVATE
                    , -1, 0);
    return space;
}


void* allocate()
{
    auto space = tryAllocate1Gb();
    auto l = mlock(space,SIZE);
    if(space != (void*) -1 && l == 0)
    {
        std::cout << "Allocated " << SIZE_GB << "GB using 1GB pages" << endl;
        return space;
    }
    space = tryAllocate2Mb();
    l = mlock(space,SIZE);
    if(space != (void*) -1 && l == 0)
    {
        std::cout << "Allocated " << SIZE_GB << "GB using 2MB pages" << endl;
        return space;
    }
    space = tryAllocate4Kb();
    l = mlock(space,SIZE);
    if(space != (void*) -1 && l == 0)
    {
        std::cout << "Allocated " << SIZE_GB << "GB using 4KB pages" << endl;
        return space;
    }
        std::cout << "Failed to allocate " << SIZE_GB << "GB" << endl;
    return nullptr;
}

std::vector<uint64_t> getUsedSets(AddressSet addressSet)
{
    std::vector<uint64_t> usedSets;
    for(auto list : addressSet)
    {
        if(!list.second.empty())
        {
            usedSets.push_back(list.first);
        }
    }
    return usedSets;
}

uint64_t frameNumberFromPagemap(uint64_t value) {
    return value & ((1ULL << 54) - 1);
}

void initPagemap() {
    g_pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
    assert(g_pagemap_fd >= 0);
}

uint64_t getPhysicalAddr(uint64_t virtualAddr) {
    uint64_t value;
    off_t offset = (virtualAddr / 4096) * sizeof(value);
    int got = pread(g_pagemap_fd, &value, sizeof(value), offset);
    assert(got == 8);

    // Check the "page present" flag.
    assert(value & (1ULL << 63));
    uint64_t frame_num = frameNumberFromPagemap(value);
    return (frame_num * 4096) | (virtualAddr & (4095));
}

void access(uint64_t addr)
{
    volatile uint64_t *p = (volatile uint64_t *) addr;
    for (unsigned int i = 0; i < NUM_ACCESS; i++)
    {
        _mm_clflush((void*)p);
        _mm_lfence();
        *p;
        _mm_lfence();
    }
}

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                  group_fd, flags);
    return ret;
}



int setupMeasure(int cpuid, unsigned int channel,unsigned int rank, unsigned int bank, bool bankGroup=false)
{
    struct perf_event_attr pe;
    int fd;

    memset(&pe, 0, sizeof(struct perf_event_attr));

    auto imcs = SysInfo::getImcs();
    auto imc = imcs[channel];
    pe.type = SysInfo::getTypeOfImc(imc);
    pe.size = sizeof(struct perf_event_attr);
    //pe.config = (0b11 << 8) | 0x04; // CAS_COUNT_READ
    //pe.config = (0b00001100 << 8) | 0x04; // CAS_COUNT_WR
    //unsigned int bankBits = 0b00010000;//all banks
    unsigned int bankBits = 0;
    if(bankGroup==true)
    {
        bankBits = 0b00010001 + bank;
    }
    else
    {
        bankBits = bank;
    }
    bankBits = bankBits << 8;
    auto rankBits = 0xb0 + rank;
    auto bits = bankBits | rankBits;
    pe.config = bits ; //umask first followed by event id
    // exact config is in cat /sys/devices/cpu/format/event
    pe.disabled = 1;
    pe.sample_type=PERF_SAMPLE_IDENTIFIER;
    pe.inherit=0;
    pe.exclude_guest=1;

    //how to get per-socket numbers?
    // choose the cpu id in syscall
    //pid = -1 cpu >= 0 -> global measurement
    fd = perf_event_open(&pe, -1, cpuid, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }

    return fd;
}

void startMeasure(int fd)
{
    _mm_mfence();
    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
}

long long stopMeasure(int fd)
{
    long long count;
    _mm_mfence();
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    read(fd, &count, sizeof(long long));
    close(fd);
    return count;
}

uint64_t getRandomAddress(uint64_t base, uint64_t size)
{
    size_t part1 = static_cast<size_t>(rand());
    size_t part2 = static_cast<size_t>(rand());
    size_t offset = ((part1 << 32ULL) | part2) % size;
    auto clSize = 64ULL;
    offset = (offset / clSize) * clSize;
    return  base + offset;
}

uint64_t getNextAddress(uint64_t oldAddr, uint64_t base, uint64_t size)
{
    static const size_t MIN_SHIFT = 6;
    static size_t shift = MIN_SHIFT;
    static uint64_t baseAddr = 0;
    if(shift == MIN_SHIFT)
    {
        baseAddr = oldAddr;
    }
    auto oldPhys = getPhysicalAddr(baseAddr);
    auto addr = baseAddr ^ (1ULL << shift);
    if(addr >= base && addr < base+size)
    {
        volatile uint8_t* p;
        p = (uint8_t*) addr;
        *p;
        auto phys = getPhysicalAddr(addr);
        auto diff = std::bitset<64>(phys) ^ std::bitset<64>(oldPhys);
        if(diff.count() > 1)
        {
            //moved into a new frame
            shift = MIN_SHIFT;
            return getRandomAddress(base,size);
        }
        else
        {
            shift++;
            return addr;
        }
    }
    else
    {
        shift = MIN_SHIFT;
        return getRandomAddress(base,size);
    }
}

uint64_t getUsableBits(uint64_t removeFront, uint64_t removeBack)
{
    uint64_t usableBits = 64ULL - removeFront - removeBack;
    return  usableBits;
}

void cleanAddresses(std::map<size_t,std::vector<size_t>>& addresses,
                    uint64_t removeFront, uint64_t removeBack)
{
    auto usableBits = getUsableBits(removeFront,removeBack);
    uint64_t mask = 1;
    mask = mask << usableBits;
    mask = mask - 1ULL;

    for(auto& list : addresses)
    {
        for(auto& a : list.second)
        {
            a >>= removeFront;
            a &= mask;
        }
    }
}

std::vector<Solver::Solution> calculateAddressingFunction(const std::map<size_t,std::vector<size_t>>& addresses, size_t addrFuncBits, size_t usableBits)
{
    std::vector<Solver::Solution> sList;
    for(size_t bit = 0; bit < addrFuncBits; bit++)
    {
        std::vector<uint64_t> matrix;
        Solver s;
        for(auto adrList : addresses)
        {
            uint64_t mask = 1ULL << bit;
            auto bitValue = (adrList.first & mask) >> bit;
            for(auto row : adrList.second)
            {
                auto rowWithResult = (row << 1ULL) | bitValue;
                matrix.push_back(rowWithResult);
            }
        }
        s.solve(matrix,usableBits);
        auto sol = s.getSolution(matrix);
        sList.push_back(sol);
    }
    return sList;
}

void printSolution(const Solver::Solution& s, size_t offset)
{
    if(s.exists)
    {
        std::cout << "Involved bits: ";
        for (auto b : s.involvedBits)
        {
            std::cout << offset + b << " ";
        }
        std::cout << std::endl;
        std::cout << "Uninvolved bits: ";
        for (auto b : s.uninvolvedBits)
        {
            std::cout << offset + b << " ";
        }
        std::cout << std::endl;
        std::cout << "Unknown bits: ";
        for (auto b : s.unknownBits)
        {
            std::cout << offset + b << " ";
        }
        std::cout << std::endl;
    }
    else
    {
        std::cout << "No solution exists" << std::endl;
    }
}

void printSolutions(const std::vector<Solver::Solution> sList, size_t offset)
{
    for(size_t i = 0; i< sList.size(); i++)
    {
        cout << "Bit " << i << ": " << endl;
        printSolution(sList[i],offset);
    }
}


std::map<size_t,std::vector<uint64_t>> compactSets(const std::map<size_t,std::vector<uint64_t>>& addresses)
{
    std::map<size_t,std::vector<uint64_t>> newAddresses;
    size_t newIdx = 0;
    for(const auto& adr : addresses)
    {
        if(!adr.second.empty())
        {
            newAddresses.insert(make_pair(newIdx,adr.second));
            newIdx++;
        }
    }
    return newAddresses;
}


void prepareSolvePrint(AddressSet adrs,size_t removeFront, size_t removeBack)
{
    cleanAddresses(adrs,removeFront,removeBack);
    adrs = compactSets(adrs);
    auto expectedBits = static_cast<size_t>(ceil(log2(adrs.size())));
    auto cSol = calculateAddressingFunction(adrs,expectedBits,getUsableBits(removeFront,removeBack));
    printSolutions(cSol,removeFront);
}

bool checkConfig(unsigned int nodeid)
{
    std::vector<std::vector<unsigned int>> tadRegions;
    for(unsigned int c = 0; c < 2; c++)
    {
        auto reg = SysInfo::readTadRegion(nodeid,c,0);
        if(reg != 0)
        {
            tadRegions.push_back(std::vector<unsigned int>());
            tadRegions[c].push_back(reg);
            for(unsigned int r = 1; r < 2; r++)
            {
                auto reg = SysInfo::readTadRegion(nodeid,c,r);
                if(SysInfo::getRegionLimitAddress(reg) != SysInfo::getRegionLimitAddress(tadRegions[c][0]))
                {
                    tadRegions[c].push_back(reg);
                }
            }
        }
    }
    if(tadRegions.size() == 0)
    {
        std::cout << "Found no controllers" << endl;
        return false;
    }
    std::cout << "Found " << tadRegions.size() << " controllers. ";
    if((tadRegions.size() == 1 && tadRegions[0].size() == 1) ||
            (tadRegions.size() == 2 && tadRegions[0].size() == 1 && tadRegions[1].size() == 1))
    {
        std::cout << "Found one TAD region in each controller." << endl;
    }
    else
    {
        cout << "Multiple TAD regions are not supported." << endl;
        return false;
    }

    std::vector<unsigned int> numChannels;
    for(unsigned int i = 0; i < tadRegions.size(); i++)
    {
        auto cn = SysInfo::getNumberOfChannels(nodeid,i);
        numChannels.push_back(cn);
    }

    auto firstOffset = SysInfo::readPerChannelOffsetForTadRegion(nodeid,0,0,0);
    for(unsigned int i = 0; i < numChannels.size(); i++)
    {
        for(unsigned int c = 0; c < numChannels[i]; c++)
        {
            auto offset = SysInfo::readPerChannelOffsetForTadRegion(nodeid,i,c,0);
            if(offset != firstOffset)
            {
               cout << "Different TAD offsets are not supported." << endl;
               return false;
            }
        }
    }
    cout << "TAD offset OK" << endl;

    auto firstRegion = SysInfo::getRankInterleaveingRegionLimit(SysInfo::readRankInterleavingRegion(nodeid,0,0,0));
    for(unsigned int i = 0; i < numChannels.size(); i++)
    {
        for (unsigned int c = 0; c < numChannels[i]; c++)
        {
            //Must be the same on all channels.
            //If the first two regions have the same limit there is only one region defined.
            for(unsigned int r = 0; r < 2; r++)
            {
                auto region = SysInfo::getRankInterleaveingRegionLimit(SysInfo::readRankInterleavingRegion(nodeid,i,c,r));
                if(firstRegion != region)
                {
                    cout << "Different rank regions are not supported." << endl;
                    return false;
                }
            }
        }
    }
     cout << "Rank configuration OK" << endl;
     return true;
}

int main(int argc, char *argv[])
{

    int opt;
    bool doCheck = true;
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
        case 'd': doCheck = false; break;
        default:
            std::cout <<"Usage: " << argv[0] << "[-d]\n"
                     << "Must be run as root to resolve physical addresses\n";
            exit(EXIT_FAILURE);
        }
    }

    unsigned int nodeid;
    unsigned int cpuid;
    auto status = syscall(SYS_getcpu, &cpuid, &nodeid, nullptr);
    if(status == -1)
    {
        cout << "Can not determine node id" << endl;
        return EXIT_FAILURE;
    }
    cout << "Running on socket " << nodeid << endl;


    if(doCheck)
    {
        if(!checkConfig(nodeid))
        {
            return EXIT_FAILURE;
        }
    }




    initPagemap();
    auto space = (uint64_t) allocate();
    if(space == 0)
    {
        return EXIT_FAILURE;
    }
    srand(3344);

    std::set<uint64_t> usedAddresses;
    std::map<size_t,std::vector<uint64_t>>channelAddresses;
    std::map<size_t,std::vector<uint64_t>>rankAddresses;
    std::map<size_t,std::vector<uint64_t>>bankAddresses;
    std::map<size_t,std::vector<uint64_t>>bankGroupAddresses;



    const size_t NUM_ADDRESS_TOTAL = 1000;
    auto adr=space;

    while(usedAddresses.size() < NUM_ADDRESS_TOTAL)
    {
        //auto adr = getRandomAddress(space,SIZE);
        adr = getNextAddress(adr,space,SIZE);
        auto physicalAddress = getPhysicalAddr(adr);
        cout << bitset<64>(physicalAddress) << endl;
        if(usedAddresses.count(physicalAddress) > 0)
        {
            continue;
        }
        usedAddresses.insert(physicalAddress);
        int identifiedChannel = -1;
        int identifiedRank = -1;
        int identifiedBank = -1;
        int identifiedBankGroup = -1;
        bool found = false;
        for(unsigned int channel = 0; channel < 4 && !found; channel++)
        {
            for(unsigned int rank = 0; rank < 8 && !found; rank++)
            {

                for(unsigned int bank = 0; bank < 16 && !found; bank++)
                {
                    usleep(1);
                    auto fd = setupMeasure(cpuid,channel,rank,bank);
                    startMeasure(fd);
                    access(adr);
                    auto count = stopMeasure(fd);
                    if (count >= 0.9*NUM_ACCESS)
                    {
                        identifiedChannel = channel;
                        identifiedRank = rank;
                        identifiedBank = bank;
                        found = true;
                    }
                }

                for(unsigned int bankGroup = 0; bankGroup < 4; bankGroup++)
                {
                    auto fd = setupMeasure(cpuid,channel,rank,bankGroup,true);
                    startMeasure(fd);
                    access(adr);
                    auto count = stopMeasure(fd);
                    //cout << count << endl;
                    if (count >= 0.9*NUM_ACCESS)
                    {
                        identifiedBankGroup = bankGroup;
                        break;
                    }
                }
            }
        }
        if(found)
        {
            channelAddresses[identifiedChannel].push_back(physicalAddress);
            //rankAddresses[std::make_tuple(identifiedChannel,identifiedRank)].push_back(physicalAddress);
            rankAddresses[identifiedRank].push_back(physicalAddress);
            bankAddresses[identifiedBank].push_back(physicalAddress);
            bankGroupAddresses[identifiedBankGroup].push_back(physicalAddress);
        }
        else
        {
            std::cout << "No set found";
        }
    }

    for (size_t i = 0 ;i < 4; i++)
    {
        std::cout << "caputured: " << channelAddresses[i].size() << " addresses on channel " << i << endl;
    }

    for (size_t j = 0 ;j < 8; j++)
    {
        std::cout << "caputured: " << rankAddresses[j].size() << " addresses on rank " << j << endl;
    }
    for(size_t k = 0; k < 16; k++)
    {
        std::cout << "caputured: " << bankAddresses[k].size() << " addresses on bank " << k << endl;
    }
    for(size_t k = 0; k < 4; k++)
    {
        std::cout << "caputured: " << bankGroupAddresses[k].size() << " addresses on bankGroup " << k << endl;
    }

    uint64_t andAll = std::numeric_limits<uint64_t>::max();
    uint64_t orAll = 0;
    for(auto a : usedAddresses)
    {
        orAll |= a;
        andAll &= a;
    }
    // bits with value 0 in orAll are 0 in all addresses
    // bits with value 1 in andAll are 1 in all addresses
    std::bitset<64> orAllBits(orAll);
    std::bitset<64> andAllBits(andAll);
    std::cout << "and all bits: " << andAllBits  << endl;
    std::cout << "or all bits:  " << orAllBits  << endl;
    std::bitset<64> unknownBits = 0;
    for(size_t i = 0; i < 64; i++)
    {
        if(orAllBits[i] == 0 || andAllBits[i] == 1)
        {
            unknownBits[i] = 1;
        }
    }
    std::cout << "Unknown bits: " << unknownBits  << endl;


    uint64_t removeFront = 0;
    for(size_t i = 0; i < 64; i++)
    {
        if(unknownBits[i] == 0)
        {
            removeFront = i;
            break;
        }
    }
    std::cout << "remove " << removeFront << " from front" << endl;

    uint64_t removeBack = 0;
    for(size_t i = 64; i-- > 0;)
    {
        if(unknownBits[i] == 0)
        {
            removeBack = 63 - i ;
            break;
        }
    }
    std::cout << "remove " << removeBack << " from back" << endl;

    cout << "Channels" << endl;
    prepareSolvePrint(channelAddresses,removeFront,removeBack);
    cout << endl;

    cout << "Ranks" << endl;
    auto setNums = getUsedSets(rankAddresses);
    cout << "Used ranks: ";
    for(auto s : setNums)
    {
        cout << s << " ";
    }
    cout << endl;
    prepareSolvePrint(rankAddresses,removeFront,removeBack);
    cout << endl;

    cout << "Banks" << endl;
    prepareSolvePrint(bankAddresses,removeFront,removeBack);
    cout << endl;

    cout << "Bank Groups" << endl;
    prepareSolvePrint(bankGroupAddresses,removeFront,removeBack);
    cout << endl;

    return 0;
}


