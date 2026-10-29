/* Self-update, as in RadioScan: ask GitHub for the latest release, and if it is
 * newer than HI5_VERSION, download this device's package and swap the new files
 * into the app folder. The song (hi5.sav) and any samples you added stay as
 * they are. ui.c reads the status; main_trimui.c drives it.
 *
 * The package is the one the app was installed from: HI5-brick.zip (or
 * -brickpro.zip in the Brick Pro build) for TrimUI OS, and
 * HI5-mustardos-brick.muxapp (or -brickpro) for MustardOS, told apart by the
 * mux_launch.sh that only the MustardOS package has. */
#pragma once
#include <stddef.h>

#define HI5_REPO "sydilaxe/Trimui-Hi5-dev"
#ifdef HI5_BRICKPRO
#define HI5_DEVICE "brickpro"
#else
#define HI5_DEVICE "brick"
#endif
/* the release files, each with the folder its zip holds the app in */
#define HI5_ASSET "HI5-" HI5_DEVICE ".zip"                    /* TrimUI OS */
#define HI5_ZIP_DIR "Apps/HI5/"                               /* as TrimUI OS lists apps */
#define HI5_ASSET_MUSTARD "HI5-mustardos-" HI5_DEVICE ".muxapp" /* MustardOS */
#define HI5_ZIP_DIR_MUSTARD "HI5/"            /* MustardOS unpacks a .muxapp into its application folder */
#define HI5_MUSTARD_LAUNCHER "mux_launch.sh"  /* only in the MustardOS package */

/* exit status that tells launch.sh to start the app again (the new version) */
#define RELAUNCH_STATUS 75

enum {
    UP_IDLE,         /* not checked yet */
    UP_CHECKING,
    UP_CURRENT,      /* this is the latest release */
    UP_AVAILABLE,    /* a newer release is out: latest, url */
    UP_CHECK_FAILED,
    UP_INSTALLING,   /* progress 0..100 */
    UP_INSTALLED,    /* the new files are in place: relaunch */
    UP_FAILED        /* the install failed: err (the old version is untouched) */
};

typedef struct UpdateStatus {
    volatile int state;
    volatile int progress;
    char latest[32]; /* "0.3.1" */
    char url[1024];  /* the release's update_asset() */
    char err[160];
} UpdateStatus;

UpdateStatus *update_status(void);
/* pick the package from what is in app_dir (TrimUI OS unless it has mux_launch.sh);
 * returns 1 for MustardOS. Call before checking. */
int update_detect_package(const char *app_dir);
const char *update_asset(void); /* the release file this copy updates from */
void update_check_start(void);                  /* in the background: CHECKING -> CURRENT / AVAILABLE / CHECK_FAILED */
void update_install_start(const char *app_dir); /* from AVAILABLE or FAILED: INSTALLING -> INSTALLED / FAILED */

/* blocking versions, for tools/ and tests; 0 on success */
int update_check(void);
int update_install(const char *app_dir);

/* true if the dotted version a ("v0.3.1" or "0.3.1") is newer than b */
int version_newer(const char *a, const char *b);
/* unpack the app folder of a zip (Apps/HI5/, or HI5/ for MustardOS) into dir, keeping the user's files; 0 on success */
int update_unzip(const unsigned char *zip, long len, const char *dir, char *err, size_t errlen);
