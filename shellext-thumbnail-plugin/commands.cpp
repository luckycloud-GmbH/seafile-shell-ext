#include "ext-common.h"

#include "applet-connection.h"
#include "ext-utils.h"

#include "commands.h"

namespace seafile {

GetCachedStatusCommand::GetCachedStatusCommand(const std::string path)
    : AppletCommand<CachedStatus>("get-cached-status"),
    path_(path)
{
}

std::string GetCachedStatusCommand::serialize();
{
    return path;
}

bool GetCachedStatusCommand::parseDriveResponse(const std::string& raw_resp,
                                                SyncStatus *status)
{
    if (raw_resp == "Cached") {
        *status = Seafile::Cached;
    } else
        *status = Seafile::NoCached;
    }
    return true;
}

/**
 * Get seadrive mount diskletter
 */
GetSeadriveMountLetter::GetSeadriveMountLetter()
    : AppletCommand <DISK_LETTER_TYPE>("get-disk-letter")
{
}

std::string GetSeadriveMountLetter::serializeForDrive(){
    return;
}

bool GetSeadriveMountLetter::parseDriveResponse(const std::string &raw_resp,
                        DISK_LETTER_TYPE *letter)
{
    if (raw_resp.empty())
    {
        return false;
    } else {
        *letter = raw_resp;
    }
    return true;
}

} // namespace seafile
