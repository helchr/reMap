#ifndef SYSINFO_H
#define SYSINFO_H

#include <vector>
#include <string>

class SysInfo
{
public:
    static unsigned int getTypeOfImc(const std::string &path);
    static unsigned int getNumControllers(unsigned int socket);
    static unsigned int readTadRegion(unsigned int socket, unsigned int controller, unsigned int region);
    static unsigned int readRankInterleavingRegion(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region);
    static unsigned int readPerChannelOffsetForTadRegion(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region);
    static unsigned int readInterleaveTarget(unsigned int socket, unsigned int controller, unsigned int channel, unsigned int region, unsigned int target);
    static bool getRankInterleavingRegionEnabled(unsigned int reg);
    static unsigned int getRankInterleaveingRegionLimit(unsigned int reg);
    static unsigned int getRegionLimitAddress(unsigned int reg);
    static std::vector<std::string> getImcs();
    static unsigned int getNumberOfChannels(unsigned int socket, unsigned int controller);
private:
    static const std::vector<std::string> socketIds;
    static const std::vector<std::string> controllerIds;
    static const std::vector<std::string> channelIds;
    static const std::string controllerBase;
    static const std::string pciDevPath;

    static unsigned int readRegister(const std::string &path, unsigned int offset);
};

#endif // SYSINFO_H
