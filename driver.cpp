#include <string>
#include <string_view>
#include <sys/stat.h>
#include <dlfcn.h>
#include <android/api-level.h>
#include <android/log.h>
#include <android_linker_ns.h>
#include "driver.h"



/**
 * @brief Holds the parameters for the main libvulkan.so hook
 * @note See comments for adrenotools_open_libvulkan as a reference for member variables
 */
struct MainHookParam {
    int featureFlags;
    std::string tmpLibDir;
    std::string hookLibDir;
    std::string customDriverDir;
    std::string customDriverName;
    std::string fileRedirectDir;

    MainHookParam(int featureFlags, const char *tmpLibDir, const char *hookLibDir, const char *customDriverDir, const char *customDriverName, const char *fileRedirectDir)
        : featureFlags(featureFlags),
          tmpLibDir(tmpLibDir ? tmpLibDir : ""),
          hookLibDir(hookLibDir),
          customDriverDir(customDriverDir ? customDriverDir : ""),
          customDriverName(customDriverName ? customDriverName : ""),
          fileRedirectDir(fileRedirectDir ? fileRedirectDir : "") {}
};

void *adrenotools_open_libvulkan(int dlopenMode, int featureFlags, const char *tmpLibDir, const char *hookLibDir, const char *customDriverDir, const char *customDriverName, const char *fileRedirectDir) {
    // Bail out if linkernsbyapss failed to load, this probably means we're on api < 28
    if (!linkernsbypass_load_status())
        return nullptr;

    // Verify that params for specific features are only passed if they are enabled
    if (!(featureFlags & ADRENOTOOLS_DRIVER_FILE_REDIRECT) && fileRedirectDir)
        return nullptr;

    if (!(featureFlags & ADRENOTOOLS_DRIVER_CUSTOM) && (customDriverDir || customDriverName))
        return nullptr;

    // Verify that params for enabled features are correct
    struct stat buf{};

    if (featureFlags & ADRENOTOOLS_DRIVER_CUSTOM) {
        if (!customDriverName || !customDriverDir)
            return nullptr;

        if (stat((std::string(customDriverDir) + customDriverName).c_str(), &buf) != 0)
            return nullptr;
    }

    // Verify that params for enabled features are correct
    if (featureFlags & ADRENOTOOLS_DRIVER_FILE_REDIRECT) {
        if (!fileRedirectDir)
            return nullptr;

        if (stat(fileRedirectDir, &buf) != 0)
            return nullptr;
    }

    // Will be destroyed by the libmain_hook.so destructor on unload
    auto *hookParam{new MainHookParam(featureFlags, tmpLibDir, hookLibDir, customDriverDir, customDriverName, fileRedirectDir)};

    return linkernsbypass_dlopen_unique_hooked("/system/lib64/libvulkan.so", tmpLibDir, dlopenMode, hookLibDir, "libmain_hook.so", nullptr, true, reinterpret_cast<void *>(hookParam));
}
