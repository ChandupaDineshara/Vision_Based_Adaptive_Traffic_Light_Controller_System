/*
 * secrets.example.h - copy this file to secrets.h and fill it in. secrets.h is ignored by git, so
 * the Wi-Fi password never reaches the repository. Only the "tuning" build (UPLOAD_PHOTOS = 1)
 * needs it.
 */
#ifndef SECRETS_H
#define SECRETS_H

#define WIFI_SSID      "DESKTOP-IMH3MOH 2660"
#define WIFI_PASSWORD  "g42312A("

/* Address of the laptop running tools/receive_photos.py.
 * On a Windows "Mobile hotspot" the laptop is normally 192.168.137.1 (check with ipconfig). */
#define UPLOAD_URL     "http://192.168.137.1:8000/upload"

#endif /* SECRETS_H */
