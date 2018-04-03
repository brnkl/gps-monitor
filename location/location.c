#include "location.h"
#include "interfaces.h"
#include "legato.h"
#include "util.h"

le_posCtrl_ActivationRef_t posCtrlRef;
double lat, lon, horizAccuracy;
// we initially set these pointers to null
// so we can tell if there has never been a reading
double *latPtr = NULL, *lonPtr = NULL, *horizAccuracyPtr = NULL;
uint64_t lastReadingDatetime = 0;
le_timer_Ref_t pollingTimer;

bool hasReading() {
  return latPtr != NULL && lonPtr != NULL && horizAccuracyPtr != NULL;
}

bool canGetLocation() {
  return hasReading() && posCtrlRef != NULL;
}

void setLocationPtrs() {
  horizAccuracyPtr = &horizAccuracy;
  latPtr = &lat;
  lonPtr = &lon;
}

le_result_t brnkl_gps_getCurrentLocation(double* latitude,
                                         double* longitude,
                                         double* horizontalAccuracy,
                                         uint64_t* lastReading) {
  if (!canGetLocation()) {
    return LE_UNAVAILABLE;
  }
  *latitude = *latPtr;
  *longitude = *lonPtr;
  *horizontalAccuracy = *horizAccuracyPtr;
  *lastReading = lastReadingDatetime;
  return LE_OK;
}

/**
  * Attempts to use the GPS to find the current latitude, longitude and
 * horizontal accuracy within
  * the given timeout constraints.
  *
  * @return
  *      - LE_OK on success
  *      - LE_UNAVAILABLE if positioning services are unavailable
  *      - LE_TIMEOUT if the timeout expires before successfully acquiring the
 * location
  *
  * @note
  *      Blocks until the location has been identified or the timeout has
 * occurred.
  */
void getLocation(le_timer_Ref_t timerRef) {
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
    // set these so we know there is a reading
    lastReadingDatetime = GetCurrentTimestamp();
    if (!hasReading()) {
      setLocationPtrs();
    }
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

le_result_t gps_init() {
  pollingTimer = le_timer_Create("GPS polling timer");
  le_timer_SetHandler(pollingTimer, getLocation);
  le_timer_SetRepeat(pollingTimer, 1);
  le_timer_SetMsInterval(pollingTimer, 0);
  posCtrlRef = le_posCtrl_Request();
  le_timer_Start(pollingTimer);
  return posCtrlRef != NULL ? LE_OK : LE_UNAVAILABLE;
}

COMPONENT_INIT {
  gps_init();
}
