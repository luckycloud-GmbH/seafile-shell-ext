#include "ext-common.h"
#include "ext-utils.h"
#include "shell-ext.h"
#include "log.h"
#include "commands.h"
#include "i18n.h"

#define SEAFILE_TR(x) seafile::getString((x)).c_str()

namespace utils = seafile::utils;

namespace {

bool shouldIgnorePath(const std::string& path)
{
    /* Show no menu for drive root, such as C: D: */
    if (path.size() <= 3) {
        return TRUE;
    }

    /* Ignore flash disk, network mounted drive, etc. */
    // if (GetDriveType(path.substr(0, 3).c_str()) != DRIVE_FIXED) {
    //     return TRUE;
    // }

    return FALSE;
}

const char *kMainMenuNameSeafile = L"luckycloud";
const char *kMainMenuNameSeaDrive = L"luckyDrive";

}


STDMETHODIMP ShellExt::Initialize(LPCITEMIDLIST pIDFolder,
                                   LPDATAOBJECT pDataObj,
                                   HKEY  hRegKey)
{
    return Initialize_Wrap(pIDFolder, pDataObj, hRegKey);
}

STDMETHODIMP ShellExt::Initialize_Wrap(LPCITEMIDLIST folder,
                                        LPDATAOBJECT data,
                                        HKEY /* hRegKey */)
{
    FORMATETC format = {CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
#if defined(_MSC_VER)
    STGMEDIUM stg = {TYMED_HGLOBAL, NULL };
#else
    STGMEDIUM stg = {TYMED_HGLOBAL, {L'\0'}, NULL};
#endif
    HDROP drop;
    UINT count;
    UINT size;
    HRESULT result = S_OK;
    wchar_t path_dir_w[4096];
    std::unique_ptr<wchar_t[]> path_w;

    active_menu_items_.clear();

    /* 'folder' param is not null only when clicking at the foler background;
       When right click on a file, it's NULL */
    if (folder) {
        if (SHGetPathFromIDListW(folder, path_dir_w)) {
            path_ = utils::normalizedPath(utils::wStringToUtf8(path_dir_w));
        }
    }

    /* if 'data' is NULL, then it's a background click, we have set
     * path_ to folder's name above, and the Init work is done */
    if (!data)
        return S_OK;

    /* 'data' is no null, which means we are operating on a file. The
     * following lines until the end of the function is used to extract the
     * filename of the current file. */
    if (FAILED(data->GetData(&format, &stg)))
        return E_INVALIDARG;

    drop = (HDROP)GlobalLock(stg.hGlobal);
    if (!drop)
        return E_INVALIDARG;

    // When the function copies a file name to the buffer, the return value is a
    // count of the characters copied, not including the terminating null
    // character.
    count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
    if (count == 0) {
        result = E_INVALIDARG;
    } else if (count > 1) {
        result = S_FALSE;
    } else {
        size = DragQueryFileW(drop, 0, NULL, 0);
        if (!size) {
            result = E_INVALIDARG;
        } else {
            path_w.reset(new wchar_t[size+1]);
            if (!DragQueryFileW(drop, 0, path_w.get(), size+1))
                result = E_INVALIDARG;
        }
    }

    GlobalUnlock(stg.hGlobal);
    ReleaseStgMedium(&stg);

    if (result == S_OK) {
        path_ = utils::normalizedPath(utils::wStringToUtf8(path_w.get()));
    }

    return result;
}

STDMETHODIMP ShellExt::QueryContextMenu(HMENU hMenu,
                                        UINT indexMenu,
                                        UINT idCmdFirst,
                                        UINT idCmdLast,
                                        UINT uFlags)
{
    return QueryContextMenu_Wrap(hMenu, indexMenu, idCmdFirst, idCmdLast, uFlags);
}


STDMETHODIMP ShellExt::QueryContextMenu_Wrap(HMENU menu,
                                              UINT indexMenu,
                                              UINT first_command,
                                              UINT last_command,
                                              UINT flags)
{
    if (!seafile::utils::isShellExtEnabled()) {
        return S_OK;
    }
    /* do nothing when user is double clicking */
    if (flags & CMF_DEFAULTONLY)
        return S_OK;

    if (shouldIgnorePath(path_)) {
        return S_OK;
    }

    std::string path_in_repo;
    seafile::RepoInfo repo;
    if (!pathInRepo(path_, &path_in_repo, &repo) || path_in_repo.size() <= 1) {
        return S_OK;
    }

    next_active_item_ = 0;

    main_menu_ = menu;
    first_ = first_command;
    last_ = last_command;
    index_ = 0;

    buildSubMenu(path_, repo, path_in_repo);

    if (!insertMainMenu(repo.is_seadrive ? kMainMenuNameSeaDrive : kMainMenuNameSeafile)) {
        return S_FALSE;
    }

    return MAKE_HRESULT(
        SEVERITY_SUCCESS, FACILITY_NULL, 3 + next_active_item_);
}

void ShellExt::tweakMenu(HMENU menu)
{
    MENUINFO MenuInfo;
    MenuInfo.cbSize  = sizeof(MenuInfo);
    MenuInfo.fMask   = MIM_STYLE | MIM_APPLYTOSUBMENUS;
    MenuInfo.dwStyle = MNS_CHECKORBMP;

    SetMenuInfo(menu, &MenuInfo);
}

STDMETHODIMP ShellExt::InvokeCommand(LPCMINVOKECOMMANDINFO lpcmi)
{
    return InvokeCommand_Wrap(lpcmi);
}

// This is called when you invoke a command on the menu
STDMETHODIMP ShellExt::InvokeCommand_Wrap(LPCMINVOKECOMMANDINFO info)
{
    // see http://stackoverflow.com/questions/11443282/winapi-shell-extension-overriding-windows-command
    if (HIWORD(info->lpVerb))
        return E_INVALIDARG;

    if (path_.empty()) {
        return E_INVALIDARG;
    }

    UINT id = LOWORD(info->lpVerb);
    if (id == 0)
        return S_OK;

    id--;
    if (id > active_menu_items_.size() - 1) {
        seaf_ext_log ("invalid menu id %u", id);
        return S_FALSE;
    }

    MenuOp op = active_menu_items_[id];

    // TODO: we need to decide to send the command to seafile or seadrive
    if (op == GetShareLink) {
        seafile::GetShareLinkCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == GetInternalLink) {
        seafile::GetInternalLinkCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == LockFile) {
        seafile::LockFileCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == UnlockFile) {
        seafile::UnlockFileCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == ShareToUser) {
        seafile::PrivateShareCommand cmd(path_, false);
        cmd.send(is_seadrive_menu_);
    } else if (op == ShareToGroup) {
        seafile::PrivateShareCommand cmd(path_, true);
        cmd.send(is_seadrive_menu_);
    } else if (op == ShowHistory) {
        seafile::ShowHistoryCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == Download) {
        seafile::DownloadCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == ShowLockedBy) {
        seafile::ShowLockedByCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    } else if (op == GetUploadLink) {
        seafile::GetUploadLinkByCommand cmd(path_);
        cmd.send(is_seadrive_menu_);
    }

    return S_OK;
}

STDMETHODIMP ShellExt::GetCommandString(UINT_PTR idCmd,
                                         UINT flags,
                                         UINT FAR * reserved,
                                         LPSTR pszName,
                                         UINT cchMax)
{
    return GetCommandString_Wrap(idCmd, flags, reserved, pszName, cchMax);
}

// This is for the status bar and things like that:
STDMETHODIMP ShellExt::GetCommandString_Wrap(UINT_PTR idCmd,
                                              UINT flags,
                                              UINT FAR * /*reserved*/,
                                              LPSTR pszName,
                                              UINT cchMax)
{
    lstrcpynW((LPWSTR)pszName, L"This is Seafile help string.", cchMax);
    return S_OK;
}

STDMETHODIMP ShellExt::HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return HandleMenuMsg_Wrap(uMsg, wParam, lParam);
}

STDMETHODIMP ShellExt::HandleMenuMsg_Wrap(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    LRESULT res;
    return HandleMenuMsg2(uMsg, wParam, lParam, &res);
}

STDMETHODIMP ShellExt::HandleMenuMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
    return HandleMenuMsg2_Wrap(uMsg, wParam, lParam, pResult);
}

STDMETHODIMP ShellExt::HandleMenuMsg2_Wrap(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT *pResult)
{
    return S_OK;
}

/**
 * Add two menu seperators, with seafile menu between them
 */
bool ShellExt::insertMainMenu(const wchar_t* main_menu_name)
{
    // Insert a seperate before seafile menu
    if (!InsertMenu(main_menu_, index_++, MF_BYPOSITION |MF_SEPARATOR, 0, L""))
        return FALSE;

    MENUITEMINFO menuiteminfo;
    ZeroMemory(&menuiteminfo, sizeof(menuiteminfo));
    menuiteminfo.cbSize = sizeof(menuiteminfo);
    menuiteminfo.fMask = MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING | MIIM_ID;
    menuiteminfo.fType = MFT_STRING;
    menuiteminfo.dwTypeData = (wchar_t *)main_menu_name;
    menuiteminfo.cch = wcslen(main_menu_name);
    // menuiteminfo.hbmpItem = HBMMENU_CALLBACK;
    menuiteminfo.hSubMenu = sub_menu_;
    menuiteminfo.wID = first_;

    if (!InsertMenuItem(main_menu_, index_++, TRUE, &menuiteminfo))
        return FALSE;

    // Insert a seperate after seafile menu
    if (!InsertMenu(main_menu_, index_++, MF_BYPOSITION |MF_SEPARATOR, 0, L""))
        return FALSE;

    /* Set menu styles of submenu */
    tweakMenu(main_menu_);

    return TRUE;
}

MENUITEMINFO
ShellExt::createMenuItem(const std::string& text)
{
    MENUITEMINFO minfo;
    wchar_t *text_w = utils::utf8ToWString(text);
    memset(&minfo, 0, sizeof(minfo));
    minfo.cbSize = sizeof(MENUITEMINFO);
    minfo.fMask = MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING | MIIM_ID;
    minfo.fType = MFT_STRING;
    minfo.dwTypeData = text_w;
    minfo.cch = wcslen(text_w);
    minfo.hbmpItem = HBMMENU_CALLBACK;
    minfo.wID = first_ + 1 + next_active_item_++;

    return minfo;
}

void ShellExt::insertSubMenuItem(const std::string& text, MenuOp op)
{
    MENUITEMINFO minfo;
    minfo = createMenuItem(text);
    InsertMenuItem (sub_menu_, /* menu */
                    index_++,  /* position */
                    TRUE,      /* by position */
                    &minfo);
    free (minfo.dwTypeData);
    active_menu_items_.push_back(op);
}


void ShellExt::buildSubMenu(const std::string& path,
                            const seafile::RepoInfo& repo,
                            const std::string& path_in_repo)
{
    is_seadrive_menu_ = repo.is_seadrive;
#if 0
    if (repo.is_seadrive) {
        insertSubMenuItem(SEAFILE_TR("download"), Download);
    }
#endif
    insertSubMenuItem(SEAFILE_TR("get share link"), GetShareLink);
    if (repo.support_internal_link) {
        insertSubMenuItem(SEAFILE_TR("get internal link"), GetInternalLink);
    }

    // FIXME: seadrive client doens't support private share dialog yet.
    std::unique_ptr<wchar_t[]> path_w(utils::utf8ToWString(path_));
    bool is_dir = GetFileAttributesW(path_w.get()) & FILE_ATTRIBUTE_DIRECTORY;
    if (repo.support_private_share && is_dir) {
        insertSubMenuItem(SEAFILE_TR("share to a user"), ShareToUser);
        insertSubMenuItem(SEAFILE_TR("share to a group"), ShareToGroup);
    }

    if (!is_dir) {
        seafile::SyncStatus status =
            getRepoSyncStatus(path, repo.repo_id, path_in_repo, false);

        if (status == seafile::LockedByMe) {
            insertSubMenuItem(SEAFILE_TR("unlock this file"), UnlockFile);
        } else if (status == seafile::LockedByOthers) {
            insertSubMenuItem(SEAFILE_TR("locked by ..."), ShowLockedBy);
        } else if (status != seafile::ReadOnly) {
            insertSubMenuItem(SEAFILE_TR("lock this file"), LockFile);
        }
    }

    if (!is_dir) {
        insertSubMenuItem(SEAFILE_TR("view file history"), ShowHistory);
    }

    if (is_dir) {
        insertSubMenuItem(SEAFILE_TR("get upload link"), GetUploadLink);
    }
}
