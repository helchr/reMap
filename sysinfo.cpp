#include "sysinfo.h"
#include <vector>
#include <string>
#include <fstream>
#include <experimental/filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::experimental::filesystem;

const std::vector<std::string> SysInfo::socketIds = {"7f","ff"};
const std::vector<std::string> SysInfo::controllerIds = {"13","16"};
const std::string SysInfo::controllerBase = "0";
const std::vector<std::string> SysInfo::channelIds = {"2","3","4","5"};
const std::string SysInfo::pciDevPath = "/proc/bus/pci/";

unsigned int SysInfo::readRegister(const std::string& path, unsigned int offset)
{
    std::ifstream ifs(path,std::ios::in|std::ios::binary);
    if(!ifs.good())
    {
        throw(std::runtime_error("Can not open file at " + path));
    }
    ifs.ignore(offset);
    if(!ifs.good())
    {
        throw(std::runtime_error("Offset " + std::to_string(offset) + " is not within file " + path));
    }
    unsigned int reg = 0;
    ifs.read((char*)&reg,sizeof(reg));
    return reg;
}

unsigned int SysInfo::getNumberOfChannels(unsigned int socket, unsigned int controller)
{
    unsigned int count = 0;
    for(unsigned int i = 0; i < 4; i++)
    {
        std::string path = pciDevPath + socketIds.at(socket) +"/" + controllerIds.at(controller) + "." + channelIds.at(i);
        if(fs::exists(path))
        {
            count++;
        }
    }
    return count;
}

unsigned int SysInfo::readTadRegion(unsigned int socket, unsigned int controller, unsigned int region)
{
    std::string path = pciDevPath + socketIds[socket] +"/" + controllerIds[controller] + "." + controllerBase;
    return readRegister(path,0x80+(region*4));
}

unsigned int SysInfo::readRankInterleavingRegion(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region)
{
    std::string path = pciDevPath + socketIds.at(socket) +"/" + controllerIds.at(controller) + "." + channelIds.at(channel);
    return readRegister(path,0x108+(region*4));
}

unsigned int SysInfo::readPerChannelOffsetForTadRegion(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region)
{
    std::string path = pciDevPath + socketIds.at(socket) +"/" + controllerIds.at(controller) + "." + channelIds.at(channel);
    return readRegister(path,0x90+(region*4));
}

unsigned int SysInfo::readInterleaveTarget(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region, unsigned int target)
{
    std::string path = pciDevPath + socketIds.at(socket) +"/" + controllerIds.at(controller) + "." + channelIds.at(channel);
    return readRegister(path,0x120+(region*4*8)+(target*4));
}

bool SysInfo::getRankInterleavingRegionEnabled(unsigned int reg)
{
    unsigned int mask = 1UL<<31UL;
    return (reg&mask)>>31;
}

unsigned int SysInfo::getRankInterleaveingRegionLimit(unsigned int reg)
{
    unsigned int mask = 1 << 10;
    mask = mask -1;
    reg = (reg >> 1) & mask;
    return reg;
}

unsigned int SysInfo::getRegionLimitAddress(unsigned int reg)
{
    return reg >> 12;
}

std::vector<std::string> SysInfo::getImcs()
{
    static std::vector<std::string> imcs;
    if(imcs.empty())
    {
        std::string path = "/sys/bus/event_source/devices/";
        std::string imc = path+"uncore_imc";
        for (const auto & entry : fs::directory_iterator(path))
        {
            if(entry.path().string().rfind(imc,0) == 0)
            {
                imcs.push_back(entry.path());
            }
        }
        std::sort(imcs.begin(),imcs.end());
    }
    return imcs;
}

unsigned int SysInfo::getTypeOfImc(const std::string& path)
{
    std::ifstream ifs(path+"/type");
    unsigned int type;
    ifs >> type;
    return type;
}
