#ifndef BEACNPROFILE_H
#define BEACNPROFILE_H

#include <QString>

// Patch the BEACN mixer profile so that its "broadcast / audience" output points
// at the freshly-connected HDMI endpoint. BEACN locks its own profile XML while
// running, so the helper closes BEACN, rewrites only the broadcastOutputDeviceName
// attribute, then relaunches BEACN from the same .exe path it was running from.
//
// Profile lookup is hardcoded to the default location:
//   %USERPROFILE%\Documents\BEACN\profiles\MixerProfiles\Default Profile.beacnMixer
namespace BeacnProfile {

// Returns true if the profile was successfully updated. friendlyDeviceName is
// the Windows MMDevice friendly name (e.g. "LG TV SSCR2 (NVIDIA High Definition Audio)").
bool applyAudienceMixDevice(const QString &friendlyDeviceName);

}

#endif // BEACNPROFILE_H
