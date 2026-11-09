/* Self-update, as in RadioScan: ask GitHub for the latest release, and if it is
 * newer than HI5_VERSION, download this device's package and swap the new files
 * into the app folder. The song (hi5.sav) and any samples you added stay as
 * they are. ui.c reads the status; main_trimui.c drives it.
 *
 * The package is HI5-mustardos-brick.muxapp (or -brickpro in the Brick Pro
 * build), the MustardOS application archive. */
#pragma once
#include <stddef.h>

#define HI5_REPO "sydilaxe/Trimui-Hi5-dev"
#ifdef HI5_BRICKPRO
#define HI5_DEVICE "brickpro"
#else
#define HI5_DEVICE "brick"
#endif
/* the release file, and the folder its zip holds the app in */
#define HI5_ASSET "HI5-mustardos-" HI5_DEVICE ".muxapp"
#define HI5_ZIP_DIR "HI5/" /* MustardOS unpacks a .muxapp into its application folder */

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
const char *update_asset(void); /* the release file this copy updates from */
void update_check_start(void);                  /* in the background: CHECKING -> CURRENT / AVAILABLE / CHECK_FAILED */
void update_install_start(const char *app_dir); /* from AVAILABLE or FAILED: INSTALLING -> INSTALLED / FAILED */

/* blocking versions, for tools/ and tests; 0 on success */
int update_check(void);
int update_install(const char *app_dir);

/* true if the dotted version a ("v0.3.1" or "0.3.1") is newer than b */
int version_newer(const char *a, const char *b);
/* unpack the app folder of a zip (HI5/) into dir, keeping the user's files; 0 on success */
int update_unzip(const unsigned char *zip, long len, const char *dir, char *err, size_t errlen);
