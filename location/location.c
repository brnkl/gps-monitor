#include "interfaces.h"
#include "legato.h"
#include "util.h"

// Used to convert GPS int to double
#define GPS_DECIMAL_SHIFT 6
// Used for distance calculations
#define MIN_REQUIRED_HORIZ_ACCURACY_METRES 10  // TODO validate that this is realistic
#define POLL_PERIOD_SEC 2 * 60 // 2 minutes
#define RETRY_PERIOD_SEC 1


static le_posCtrl_ActivationRef_t posCtrlRef;
static double lat, lon, horizAccuracy;
static uint64_t lastReadingDatetime = 0;
static le_timer_Ref_t pollingTimer;

/**
 * Determine if we have a reading
 *
 * (other things make factor in here down the road)
 */
static bool hasReading() {
  return lastReadingDatetime != 0;
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
                                         uint64_t* lastReading) {
  if (!canGetLocation()) {
    return LE_UNAVAILABLE;
  }
  *latitude = lat;
  *longitude = lon;
  *horizontalAccuracy = horizAccuracy;
  *lastReading = lastReadingDatetime;
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
  bool isAccurate = rawHoriz <= MIN_REQUIRED_HORIZ_ACCURACY_METRES;
  bool resOk = result == LE_OK;
  if (resOk && isAccurate) {
    double denom = powf(10, GPS_DECIMAL_SHIFT);  // divide by this
    lat = ((double)rawLat) / denom;
    lon = ((double)rawLon) / denom;
    // no conversion required for horizontal accuracy
    horizAccuracy = (double)rawHoriz;
    lastReadingDatetime = GetCurrentTimestamp();
    LE_INFO("Got reading...");
    LE_INFO("lat: %f, long: %f, horiz: %f", lat, lon, horizAccuracy);
    le_timer_SetMsInterval(timerRef, POLL_PERIOD_SEC * 1000);
  } else {
    if (!isAccurate && resOk) {
      LE_INFO("Rejected for accuracy (%d m)", rawHoriz);
    }
    LE_INFO("Failed to get reading... retrying in %d seconds",
            RETRY_PERIOD_SEC);
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
  LE_FATAL_IF(posCtrlRef == NULL, "Couldn't activate positioning");

  pollingTimer = le_timer_Create("GPS polling timer");
  le_timer_SetHandler(pollingTimer, getLocation);
  le_timer_SetRepeat(pollingTimer, 1);
  le_timer_SetMsInterval(pollingTimer, 0);
  le_timer_Start(pollingTimer);
}

COMPONENT_INIT {
  gps_init();
}
