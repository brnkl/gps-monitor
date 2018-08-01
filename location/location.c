#include "interfaces.h"
#include "legato.h"
#include "util.h"

// Used to convert GPS int to double
#define GPS_DECIMAL_SHIFT 6
#define MIN_REQUIRED_HORIZ_ACCURACY_METRES \
  10                            // TODO validate that this is realistic
#define POLL_PERIOD_SEC 2 * 60  // 2 minutes
#define RETRY_PERIOD_SEC 1
#define DEBOUNCE_ERRORS_SEC 60

static le_posCtrl_ActivationRef_t posCtrlRef;
static le_timer_Ref_t pollingTimer;
static int lastErrorDatetime = 0;
static struct {
  double lat;
  double lon;
  double horizAccuracy;
  uint64_t datetime;
} lastReading;

/**
 * Determine if we have a reading
 *
 * (other things make factor in here down the road)
 */
static bool hasReading() {
  return lastReading.datetime != 0;
}

/**
 * Determine if we can provide an IPC caller
 * with a location
 */
static bool canGetLocation() {
  return hasReading() && posCtrlRef != NULL;
}

/**
 * IPC function to get location
 */
le_result_t brnkl_gps_getCurrentLocation(double* latitude,
                                         double* longitude,
                                         double* horizontalAccuracy,
                                         uint64_t* readingTimestamp) {
  if (!canGetLocation()) {
    return LE_UNAVAILABLE;
  }
  *latitude = lastReading.lat;
  *longitude = lastReading.lon;
  *horizontalAccuracy = lastReading.horizAccuracy;
  *readingTimestamp = lastReading.datetime;
  return LE_OK;
}

/**
 * Main polling function
 *
 * Change MIN_REQUIRED_HORIZ_ACCURACY_METRES if
 * a more/less accurate fix is required
 */
static void getLocation(le_timer_Ref_t timerRef) {
  le_timer_Stop(timerRef);
  LE_DEBUG("Checking GPS position");
  int32_t rawLat, rawLon, rawHoriz;
  le_result_t result = le_pos_Get2DLocation(&rawLat, &rawLon, &rawHoriz);
  if (posCtrlRef == NULL)
    posCtrlRef = le_posCtrl_Request();
  bool isAccurate = rawHoriz <= MIN_REQUIRED_HORIZ_ACCURACY_METRES;
  bool resOk = result == LE_OK && posCtrlRef != NULL;
  if (resOk && isAccurate) {
    double denom = powf(10, GPS_DECIMAL_SHIFT);  // divide by this
    lastReading.lat = ((double)rawLat) / denom;
    lastReading.lon = ((double)rawLon) / denom;
    // no conversion required for horizontal accuracy
    lastReading.horizAccuracy = (double)rawHoriz;
    lastReading.datetime = GetCurrentTimestamp();
    le_timer_SetMsInterval(timerRef, POLL_PERIOD_SEC * 1000);
  } else {
    int now = util_getUnixDatetime() if (now - lastErrorDatetime >=
                                         DEBOUNCE_ERRORS_SEC) {
      lastErrorDatetime = now;
      if (!isAccurate && resOk) {
        LE_INFO("Rejected for accuracy (%d m)", rawHoriz);
      }
      LE_INFO("Failed to get reading... retrying in %d seconds",
              RETRY_PERIOD_SEC);
    }

    le_timer_SetMsInterval(timerRef, RETRY_PERIOD_SEC * 1000);
  }
  le_timer_Start(timerRef);
}

/**
 * Perform all required setup
 *
 * Note that we run this on a timer to avoid
 * blocking up the main (only) thread. If this
 * was run in a while(true) that sleeps,
 * the IPC caller would be blocked indefinitely
 */
static void gps_init() {
  posCtrlRef = le_posCtrl_Request();
  pollingTimer = le_timer_Create("GPS polling timer");
  le_timer_SetHandler(pollingTimer, getLocation);
  le_timer_SetRepeat(pollingTimer, 1);
  le_timer_SetMsInterval(pollingTimer, 0);
  le_timer_Start(pollingTimer);
}

COMPONENT_INIT {
  gps_init();
}
