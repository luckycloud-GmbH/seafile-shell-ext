#ifndef SEAFILE_EXTENSION_APPLET_COMMANDS_H
#define SEAFILE_EXTENSION_APPLET_COMMANDS_H

#include <string>
#include <vector>

#include "applet-connection.h"

namespace seafile
{

    enum CachedStatus {
        NoCached = 0,
        Cached,
    };

    typedef std::string DISK_LETTER_TYPE;
/**
 * Abstract base class for all one-way commands, e.g. don't require a response
 * from seafile/seadrive client.
 */
class SimpleAppletCommand
{
public:
    SimpleAppletCommand(std::string name) : name_(name)
    {
    }

    /**
     * send the command to seafile client, don't need the response
     */
    void send()
    {
        AppletConnection::driveInstance()->sendCommand(formatDriveRequest());
    }

    std::string formatDriveRequest()
    {
        std::string body = serializeForDrive();
        if (body.empty()) {
            return name_;
        } else {
            return name_ + "\t" + body;
        }
    }

protected:
    /**
     * Prepare this command for sending through the pipe
     */
    virtual std::string serialize() = 0;
    virtual std::string serializeForDrive()
    {
        return serialize();
    }

    std::string name_;
};


/**
 * Abstract base class for all commands that also requires response from
 * seafile/seadrive client.
 */
template <class T>
class AppletCommand : public SimpleAppletCommand
{
public:
    AppletCommand(std::string name) : SimpleAppletCommand(name)
    {
    }

    /**
     * send the command to seafile client & seadrive client, and wait for the
     * response, then merge the response
     */
    bool sendAndWait(T *resp)
    {
        std::string raw_resp;
        T driveResp;
        bool driveSuccess = false;

        driveSuccess =
            AppletConnection::driveInstance()->sendCommandAndWait(
                formatDriveRequest(), &raw_resp);
        if (driveSuccess) {
            driveSuccess = parseDriveResponse(raw_resp, &driveResp);
        }

        if (driveSuccess) {
            *resp = driveResp;
            return true;
        } else {
            return false;
        }
    }

protected:
    virtual std::string serialize() = 0;
    virtual std::string serializeForDrive()
    {
        return serialize();
    }

    virtual bool parseDriveResponse(const std::string &raw_resp, T *resp)
    {
        return false;
    }

};

// getFile Cached status
class GetCachedStatusCommand : public AppletCommand <CachedStatus> {

public:
    GetCachedStatusCommand(const std::string& path);

protected:
    std::string serialize();
    std::string serializeForDrive();

    bool parseDriveResponse(const std::string &raw_resp,
                            CachedStatus *status);

private:
    std::string path_;
};

// get seadrive mount dir
class GetSeadriveMountLetter : public AppletCommand <DISK_LETTER_TYPE> {
public:
    GetSeadriveMountLetter();
protected:
    std::string serialize();
    std::string serializeForDrive();
    bool parseDriveResponse(const std::string& raw_resp,
        DISK_LETTER_TYPE* letter);
};

} // namespace seafile


#endif // SEAFILE_EXTENSION_APPLET_COMMANDS_H
