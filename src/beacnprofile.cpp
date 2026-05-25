#include "beacnprofile.h"
#include "logmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>

#include <windows.h>
#include <tlhelp32.h>

static const wchar_t kBeacnPrefix[] = L"Beacn";
static const size_t kBeacnPrefixLen = 5;

namespace {

struct RunningBeacn {
    QString exePath; // path of "Beacn.exe" if it was running, empty otherwise
    QList<DWORD> pids;
};

static RunningBeacn snapshotAndStopBeacn()
{
    RunningBeacn r;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return r;

    PROCESSENTRY32W pe = {};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsnicmp(pe.szExeFile, kBeacnPrefix, kBeacnPrefixLen) != 0) continue;
            r.pids.append(pe.th32ProcessID);

            // Capture the GUI exe path for relaunch.
            if (r.exePath.isEmpty() && _wcsicmp(pe.szExeFile, L"Beacn.exe") == 0) {
                HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                if (h) {
                    WCHAR buf[MAX_PATH];
                    DWORD sz = MAX_PATH;
                    if (QueryFullProcessImageNameW(h, 0, buf, &sz)) {
                        r.exePath = QString::fromWCharArray(buf);
                    }
                    CloseHandle(h);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    // Terminate everything matching, then wait for handles to settle.
    QList<HANDLE> handles;
    for (DWORD pid : r.pids) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
        if (!h) continue;
        if (TerminateProcess(h, 0)) {
            handles.append(h);
        } else {
            CloseHandle(h);
        }
    }
    for (HANDLE h : handles) {
        WaitForSingleObject(h, 2000);
        CloseHandle(h);
    }
    if (!r.pids.isEmpty()) {
        LogManager::info(QString("Stopped %1 Beacn process(es) for profile patch").arg(r.pids.size()));
    }
    return r;
}

static QString xmlEscapeAttribute(const QString &value)
{
    QString out;
    out.reserve(value.size());
    for (QChar ch : value) {
        ushort u = ch.unicode();
        if (u == '&')       out += "&amp;";
        else if (u == '"')  out += "&quot;";
        else if (u == '<')  out += "&lt;";
        else if (u == '>')  out += "&gt;";
        else if (u > 127)   out += QString("&#%1;").arg(u);
        else                out += ch;
    }
    return out;
}

static QString profilePath()
{
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (docs.isEmpty()) return QString();
    return QDir(docs).filePath("BEACN/profiles/MixerProfiles/Default Profile.beacnMixer");
}

static bool patchProfile(const QString &friendlyDeviceName)
{
    QString path = profilePath();
    if (path.isEmpty()) {
        LogManager::warning("Beacn profile: could not determine Documents path");
        return false;
    }
    QFile f(path);
    if (!f.exists()) {
        LogManager::warning("Beacn profile not found: " + path);
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        LogManager::warning("Beacn profile: failed to open for reading: " + path);
        return false;
    }
    QByteArray raw = f.readAll();
    f.close();
    QString content = QString::fromUtf8(raw);

    QString escaped = xmlEscapeAttribute(friendlyDeviceName);
    QRegularExpression re(QStringLiteral("(broadcastOutputDeviceName=)\"[^\"]*\""));
    int before = content.size();
    content.replace(re, QString("\\1\"%1\"").arg(escaped));
    if (content.size() == before && !content.contains("broadcastOutputDeviceName=\"" + escaped + "\"")) {
        LogManager::warning("Beacn profile: broadcastOutputDeviceName attribute not found");
        return false;
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LogManager::warning("Beacn profile: failed to open for writing: " + path);
        return false;
    }
    f.write(content.toUtf8());
    f.close();
    LogManager::info(QString("Beacn profile broadcastOutputDeviceName set to: %1").arg(friendlyDeviceName));
    return true;
}

static void startBeacn(const QString &exePath)
{
    if (exePath.isEmpty()) {
        LogManager::debug("Beacn was not running before patch; not relaunching");
        return;
    }
    qint64 pid = 0;
    if (QProcess::startDetached(exePath, QStringList(), QString(), &pid)) {
        LogManager::info(QString("Beacn restarted (pid=%1)").arg(pid));
    } else {
        LogManager::warning("Failed to relaunch Beacn: " + exePath);
    }
}

} // anonymous namespace

bool BeacnProfile::applyAudienceMixDevice(const QString &friendlyDeviceName)
{
    if (friendlyDeviceName.isEmpty()) {
        LogManager::warning("applyAudienceMixDevice: empty device name");
        return false;
    }

    RunningBeacn was = snapshotAndStopBeacn();
    // Brief settling delay — give the OS a moment to release the file lock even
    // after the process handle is signaled.
    QThread::msleep(200);

    bool ok = patchProfile(friendlyDeviceName);

    // Always restart Beacn if it was running, even if the patch failed —
    // otherwise we leave the user with no audio mixer.
    startBeacn(was.exePath);
    return ok;
}
