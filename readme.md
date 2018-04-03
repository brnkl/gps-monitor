# mangoh-gps-monitor 🌎🎯🗺

Designed to work with MangOH boards. Tested on MangOH Red with Legato 17.11. May work with other boards as well.

## Motivation 🤔

Monitoring GPS location is cool, but blocking up other important calls is way less cool, and multi-threaded programming is arguably not that cool either. This app will keep an eye on your GPS location and allow other apps to request the location.

## Setup 🛠

You are free to copy and runs this code however you would like, however we recommend using a [Git submodule](https://git-scm.com/docs/git-submodule) to stay up to date.

### Bindings 👋

To use this app, setup the required [Legato IPC bindings](http://legato.io/legato-docs/latest/basicIPC.html).

myClientApp.adef
```
...
bindings:
{
  myClientApp.myComponent.brnkl_gps -> gpsMonitor.brnkl_gps
}
...
```

Component.cdef
```
...
requires:
{
  api:
  {
    brnkl_gps.api
  }
}
...
```

Note that you will need to include this interface in your system definition (`.sdef` file), or tell `mkapp`/`mksys` where to find it with the `-i` argument. An absolute path works too but this can be a pain depending on how your project is setup.

### Example Usage ⌨️

myApp.c
```c
double lat, lon, horizAccuracy;
uint64_t lastReading;
le_result r = brnkl_gps_getLocation(&lat, &lon, &horizAccuracy, &lastReading);
LE_DEBUG("GPS reading %s", r == LE_OK ? "succeeded" : "failed");
```

That's it!
