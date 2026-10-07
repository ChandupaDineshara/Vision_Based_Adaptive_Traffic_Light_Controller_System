/*
 * VehicleCounter_SPI_SD.ino  —  Density Level Edition (SPI SD variant)
 * ─────────────────────────────────────────────────────────────────────
 * ESP32-CAM  |  Arduino IDE  |  SD card in /images/ slot, read over SPI
 *
 * Identical density-detection pipeline to the SD_MMC version — only the
 * SD access layer changed, from SD_MMC (4-bit) to SD + SPI (4 pins),
 * freeing GPIO4 and GPIO12 for the wake/done interrupt lines used in
 * later stages.
 *
 * SPI pin mapping used here (standard AI-Thinker microSD wiring):
 *   CLK  -> GPIO14
 *   MISO -> GPIO2
 *   MOSI -> GPIO15
 *   CS   -> GPIO13
 *
 * Picks a random image from /images/ on the SD card each iteration.
 * Computes a traffic DENSITY LEVEL (0–3) using mean gradient magnitude
 * — blur -> Sobel -> average -> threshold. Same algorithm, buffers,
 * and thresholds as the SD_MMC version — nothing about the analysis
 * pipeline changed.
 *
 * Density levels:
 *   L0  LOW    mean_grad < 30   road mostly empty
 *   L1  MEDIUM mean_grad 30–55  moderate traffic
 *   L2  HIGH   mean_grad 55–75  heavy traffic
 *   L3  FULL   mean_grad > 75   gridlock
 *
 * Image naming:   <anything>-count<N>.jpg   e.g. image4-count30.jpg
 * Serial output:  filename | actual_count | density_level | label
 *
 * Board:     AI Thinker ESP32-CAM
 * Tools -> PSRAM -> "OPI PSRAM"   ← REQUIRED
 * Libraries: JPEGDEC by Larry Bank (Library Manager)
 * Baud:      115200
 */

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include <JPEGDEC.h>

/* ── SPI SD pin mapping ──────────────────────────────────────────── */
#define SD_SCK    14
#define SD_MISO    2
#define SD_MOSI   15
#define SD_CS     13

/* ── image size ───────────────────────────────────────────────────── */
#define IMG_W     96
#define IMG_H     96
#define N_PIXELS  (IMG_W * IMG_H)   /* 9216 */

/* ── density thresholds (on mean Sobel gradient magnitude, 0–255) ── */
#define THRESH_LOW   30    /* below → L0 LOW    */
#define THRESH_MED   55    /* below → L1 MEDIUM */
#define THRESH_HIGH  75    /* below → L2 HIGH   */
                           /* above → L3 FULL   */

/* ── PSRAM pixel buffers ──────────────────────────────────────────── */
static uint8_t *s_rgb  = NULL;   /* 27 KB  raw RGB from JPEG decoder  */
static uint8_t *s_blur = NULL;   /*  9 KB  after Gaussian blur        */
static uint8_t *s_gmag = NULL;   /*  9 KB  Sobel magnitude            */

/* ── file list (internal DRAM, small) ────────────────────────────── */
#define MAX_FILES  64
static char s_filenames[MAX_FILES][64];
static int  s_file_count = 0;

/* ══════════════════════════════════════════════════════════════════════
 *  PSRAM ALLOCATION
 * ══════════════════════════════════════════════════════════════════════ */
static bool alloc_psram_buffers(void)
{
    s_rgb  = (uint8_t *) ps_malloc(N_PIXELS * 3);
    s_blur = (uint8_t *) ps_malloc(N_PIXELS);
    s_gmag = (uint8_t *) ps_malloc(N_PIXELS);
    return (s_rgb && s_blur && s_gmag);
}

/* ══════════════════════════════════════════════════════════════════════
 *  JPEG CALLBACK  (RGB8888 — 4 bytes per pixel, last byte = padding)
 * ══════════════════════════════════════════════════════════════════════ */
static int jpeg_draw_cb(JPEGDRAW *pDraw)
{
    uint8_t *px = (uint8_t *)pDraw->pPixels;
    for (int row = 0; row < pDraw->iHeight; row++) {
        int img_y = pDraw->y + row;
        if (img_y >= IMG_H) break;
        for (int col = 0; col < pDraw->iWidth; col++) {
            int img_x = pDraw->x + col;
            if (img_x >= IMG_W) break;
            int dst = (img_y * IMG_W + img_x) * 3;
            int src = (row * pDraw->iWidth + col) * 4;
            s_rgb[dst + 0] = px[src + 0];   /* R */
            s_rgb[dst + 1] = px[src + 1];   /* G */
            s_rgb[dst + 2] = px[src + 2];   /* B */
        }
    }
    return 1;
}

/* ══════════════════════════════════════════════════════════════════════
 *  DENSITY PIPELINE  — unchanged from the SD_MMC version
 *
 *  Step 1: RGB → grayscale  (BT.601 luma, integer)
 *  Step 2: 3×3 Gaussian blur
 *  Step 3: Sobel magnitude  (|Gx|+|Gy|)/2, averaged over all pixels
 *  Output: mean gradient → compare against 3 thresholds → level 0–3
 * ══════════════════════════════════════════════════════════════════════ */
static int compute_density_level(void)
{
    /* ── Step 1: RGB → gray, stored into s_blur temporarily ── */
    for (int i = 0; i < N_PIXELS; i++)
        s_blur[i] = (uint8_t)(((uint16_t)s_rgb[i*3]  * 77 +
                                (uint16_t)s_rgb[i*3+1]* 150 +
                                (uint16_t)s_rgb[i*3+2]* 29) >> 8);

    /* ── Step 2: 3×3 Gaussian blur  [1 2 1 / 2 4 2 / 1 2 1] / 16 ── */
    for (int y = 0; y < IMG_H; y++) {
        for (int x = 0; x < IMG_W; x++) {
            int y0 = y > 0        ? y-1 : 0;
            int y2 = y < IMG_H-1  ? y+1 : IMG_H-1;
            int x0 = x > 0        ? x-1 : 0;
            int x2 = x < IMG_W-1  ? x+1 : IMG_W-1;
            uint16_t s =
                s_blur[y0*IMG_W+x0] + 2*s_blur[y0*IMG_W+x] + s_blur[y0*IMG_W+x2] +
              2*s_blur[y *IMG_W+x0] + 4*s_blur[y *IMG_W+x] + 2*s_blur[y *IMG_W+x2] +
                s_blur[y2*IMG_W+x0] + 2*s_blur[y2*IMG_W+x] + s_blur[y2*IMG_W+x2];
            s_gmag[y*IMG_W+x] = (uint8_t)(s >> 4);
        }
    }

    /* ── Step 3: Sobel magnitude, accumulate mean ── */
    uint32_t total = 0;
    uint32_t count = 0;
    for (int y = 1; y < IMG_H-1; y++) {
        for (int x = 1; x < IMG_W-1; x++) {
            int p00=s_gmag[(y-1)*IMG_W+(x-1)], p01=s_gmag[(y-1)*IMG_W+x], p02=s_gmag[(y-1)*IMG_W+(x+1)];
            int p10=s_gmag[ y   *IMG_W+(x-1)],                              p12=s_gmag[ y   *IMG_W+(x+1)];
            int p20=s_gmag[(y+1)*IMG_W+(x-1)], p21=s_gmag[(y+1)*IMG_W+x], p22=s_gmag[(y+1)*IMG_W+(x+1)];

            int gx = -p00 + p02 - 2*p10 + 2*p12 - p20 + p22;
            int gy = -p00 - 2*p01 - p02  + p20  + 2*p21 + p22;

            int mag = (abs(gx) + abs(gy)) >> 1;
            if (mag > 255) mag = 255;
            total += (uint8_t)mag;
            count++;
        }
    }

    uint32_t mean_grad = total / count;

    if      (mean_grad < THRESH_LOW)  return 0;   /* LOW    */
    else if (mean_grad < THRESH_MED)  return 1;   /* MEDIUM */
    else if (mean_grad < THRESH_HIGH) return 2;   /* HIGH   */
    else                              return 3;   /* FULL   */
}

/* ══════════════════════════════════════════════════════════════════════
 *  JPEG LOADING FROM SD (SPI)
 * ══════════════════════════════════════════════════════════════════════ */
static JPEGDEC g_jpeg;

static bool load_and_decode_jpeg(const char *path)
{
    File f = SD.open(path);
    if (!f) {
        Serial.print("Cannot open: "); Serial.println(path);
        return false;
    }
    size_t fsize = f.size();
    if (fsize == 0 || fsize > 50000) {
        Serial.println("File empty or too large (max 50 KB)");
        f.close();
        return false;
    }

    uint8_t *jpeg_buf = (uint8_t *) ps_malloc(fsize);
    if (!jpeg_buf) {
        Serial.println("OOM: cannot allocate JPEG buffer in PSRAM");
        f.close();
        return false;
    }
    f.read(jpeg_buf, fsize);
    f.close();

    memset(s_rgb, 0, N_PIXELS * 3);
    int ok = g_jpeg.openRAM(jpeg_buf, (int)fsize, jpeg_draw_cb);
    if (!ok) { free(jpeg_buf); Serial.println("JPEG open failed"); return false; }
    g_jpeg.setPixelType(RGB8888);
    ok = g_jpeg.decode(0, 0, 0);
    g_jpeg.close();
    free(jpeg_buf);

    if (!ok) { Serial.println("JPEG decode failed"); return false; }
    return true;
}

/* ══════════════════════════════════════════════════════════════════════
 *  SD FOLDER SCAN (SPI)
 * ══════════════════════════════════════════════════════════════════════ */
static void scan_image_folder(void)
{
    s_file_count = 0;
    File dir = SD.open("/images");
    if (!dir || !dir.isDirectory()) {
        Serial.println("ERROR: /images folder not found on SD card!");
        return;
    }
    while (s_file_count < MAX_FILES) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            const char *name = entry.name();
            int len = strlen(name);
            if (len > 5 &&
                (strcasecmp(name + len - 4, ".jpg")  == 0 ||
                 strcasecmp(name + len - 5, ".jpeg") == 0) &&
                strstr(name, "count") != NULL)
            {
                snprintf(s_filenames[s_file_count], 64, "/images/%s", name);
                s_file_count++;
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("Found %d image(s) in /images/\n", s_file_count);
}

static int parse_ground_truth(const char *path)
{
    const char *p = strstr(path, "count");
    return p ? atoi(p + 5) : -1;
}

static const char *level_label(int level)
{
    switch (level) {
        case 0: return "LOW    (< 25%)";
        case 1: return "MEDIUM (25-50%)";
        case 2: return "HIGH   (50-75%)";
        case 3: return "FULL   (> 75%)";
        default: return "UNKNOWN";
    }
}

/* ══════════════════════════════════════════════════════════════════════
 *  SETUP / LOOP
 * ══════════════════════════════════════════════════════════════════════ */
void setup(void)
{
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== Traffic Density Monitor — ESP32-CAM (SPI SD) ===");

    /* PSRAM check and allocation */
    if (!psramFound()) {
        Serial.println("ERROR: PSRAM not found!");
        Serial.println("Go to: Tools -> PSRAM -> OPI PSRAM, then recompile.");
        while (true) delay(1000);
    }
    if (!alloc_psram_buffers()) {
        Serial.println("ERROR: PSRAM buffer allocation failed!");
        while (true) delay(1000);
    }
    Serial.printf("PSRAM OK — %.0f KB free after buffers\n",
                  (float)ESP.getFreePsram() / 1024.0f);

    /* SD card — SPI mode on dedicated pins (frees GPIO4/GPIO12 for later use) */
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        Serial.println("SD (SPI) mount failed! Check wiring/card is inserted.");
        while (true) delay(1000);
    }
    Serial.println("SD card mounted (SPI mode).");

    scan_image_folder();
    if (s_file_count == 0) {
        Serial.println("No valid images found. Check /images/ folder and filenames.");
        while (true) delay(1000);
    }

    randomSeed(analogRead(0) ^ (analogRead(1) << 8) ^ millis());

    Serial.println("\nDensity levels:");
    Serial.println("  L0 LOW    — mean gradient < 30   (road mostly clear)");
    Serial.println("  L1 MEDIUM — mean gradient 30-55  (moderate traffic)");
    Serial.println("  L2 HIGH   — mean gradient 55-75  (heavy traffic)");
    Serial.println("  L3 FULL   — mean gradient > 75   (gridlock)");
    Serial.println();
    Serial.println("Filename                          | Actual | Level | Label");
    Serial.println("------------------------------------------------------------------");
}

void loop(void)
{
    int idx = (int)random(s_file_count);
    const char *path = s_filenames[idx];

    if (!load_and_decode_jpeg(path)) {
        Serial.print("Skipping: "); Serial.println(path);
        delay(2000);
        return;
    }

    unsigned long t0 = millis();
    int level = compute_density_level();
    unsigned long elapsed = millis() - t0;

    int actual = parse_ground_truth(path);
    const char *short_name = strrchr(path, '/');
    short_name = short_name ? short_name + 1 : path;

    Serial.printf("%-33s | %6d | L%d    | %s  (%lums)\n",
                  short_name, actual, level, level_label(level), elapsed);

    delay(3000);
}
