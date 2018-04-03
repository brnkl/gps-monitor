#ifndef LOCATION_H
#define LOCATION_H

#include "legato.h"

// Used to convert GPS int to double
#define GPS_DECIMAL_SHIFT 6
// Used for distance calculations
#define MIN_REQUIRED_HORIZ_ACCURACY_METRES 10  // TODO validate this
#define POLL_PERIOD_SEC 2 * 60
#define RETRY_PERIOD_SEC 1

#endif  // LOCATION_H
