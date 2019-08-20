# SeaDrive Thumbnail Extension

This project is designed to create *SeaDrive Thumbnail Extension* which used to create thumbnail. Currently, MSDN recommended developers to derive Thumbnail Provider class from **IInitializeWithStream** instead of **IInitializeWithFile** because of system security. But in some conditions, we would like to deal files with its full path. Obviously, **IInitializeWithStream** is not what we want in this case. It's HARD to work with IInitializeWithFile since there is little information about how to use **IInitializeWithFile** on the Internet.

## How to use

- clone repo using following command:
```bash
git clone https://github.com/haiwen/seafile-shell-ext.git
```
- use Visual Studio to build.
- register the COM component with:

```bash
regsvr32 seadrive_thumbnail_ext.dll      // for registration
regsvr32 /u seadrive_thumbnail_ext.dll   // for unregistration
```
