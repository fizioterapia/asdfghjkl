#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MARGIN 16

#define TEXT "asdfghjkl"
#define TEXT_SIZE 69
#define TEXT_ROTATE_X SCREEN_WIDTH / 8
#define TEXT_ROTATE_Y SCREEN_HEIGHT / 8
#define TEXT_SPEED 0.25

#define PENGER_IMG "resources/penger.png"

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080

#define TARGET_FPS 165

#define GRID_COLS 64

#define BAR_HEIGHT 32
#define BAR_TEXT_SIZE 16

#define MUSIC_BAR_BANDS 128

#define VOLUME_STEP 0.2
#define VOLUME_WIDTH SCREEN_WIDTH / 4
#define VOLUME_HEIGHT 32
#define VOLUME_TEXT_SIZE 16

#define BUF_SIZE 255

const char* tracks[] = { "resources/truck_is_jarig.xm",
    "resources/cromenu#1 haschkaka.xm", "resources/lucid keygen #2.xm",
    "resources/reloaded.xm", "resources/starchip.xm",
    "resources/tomorrow land.xm",
    "resources/tuber theme #19.xm", "resources/ulfs vibrator.xm",
    "resources/zabutom&dubmood-lite_l_sa_tankar_s.xm",
    "resources/ancient temple.mod",
    "resources/zalza-pr0ndisk_outro.xm",
    "resources/zalza-tequila_groove.xm" };

typedef struct {
    Vector2 pos;
    Vector2 dir;
    int radius;
    Color col;
} Ball;

typedef struct {
    Texture* texture;
    Vector2 pos;
    Vector2 speed;
    bool flipped;
} Penger;

typedef struct {
    unsigned int currentTrack;
    float currentVolume;
    char* title;
    unsigned int rate;
    unsigned int size;
    unsigned int channels;
    float bands[MUSIC_BAR_BANDS];
} MusicData;

static MusicData md = {};

Ball balls[34 + 35] = {};
bool musicLoaded = false;
struct timeval tv = {};

static float frameTime = 0;

void DataGrabber(void* buffer, unsigned int frames)
{
    if (buffer == NULL || frames == 0)
        return;

    if (md.size == 16) {
        short* samples = (short*)buffer;

        unsigned int total_samples = frames * md.channels;

        unsigned int samples_per_band = total_samples / MUSIC_BAR_BANDS;

        int i = 0;
        while (i < MUSIC_BAR_BANDS) {
            for (int j = i * samples_per_band; j < (i + 1) * samples_per_band; j = j + md.channels) {
                md.bands[i] += fabs((float)(samples[j] / 32768.0));
                if (md.channels == 2)
                    md.bands[i] += fabs((float)(samples[j + 1] / 32768.0));
            }
            md.bands[i] = md.bands[i] / samples_per_band;
            i++;
        }
    } else if (md.size == 32) {
        float* samples = (float*)buffer;

        unsigned int total_samples = frames * md.channels;

        unsigned int samples_per_band = total_samples / MUSIC_BAR_BANDS;

        int i = 0;
        while (i < MUSIC_BAR_BANDS) {
            for (int j = i * samples_per_band; j < (i + 1) * samples_per_band; j = j + md.channels) {
                md.bands[i] += fabs(samples[j]);
                if (md.channels == 2)
                    md.bands[i] += fabs(samples[j + 1]);
            }
            md.bands[i] = md.bands[i] / samples_per_band;
            i++;
        }
    } else {
        printf("[-] unsupported md.size: %u\n", md.size);
    }

    return;
}

char* trimTitle(const char* title)
{
    char* newTitle;
    int startIndex = 0;

    int len = 0;

    while (title[len] != '\0' && title[len] != '.') {
        if (title[len] == '/' && startIndex == 0)
            startIndex = len + 1;
        ++len;
    }

    len -= startIndex;

    newTitle = malloc(sizeof(char) * len);

    memcpy(newTitle, title + (startIndex * sizeof(char)), len);
    memset(newTitle + (len * sizeof(char)), 0, 1);

    return newTitle;
}

void ChangeSong(Music* music, bool rand, bool inc)
{
    if (musicLoaded) {
        UnloadMusicStream(*music);
        musicLoaded = false;
    }

    srand((int)(GetTime() * 1000));

    if (rand)
        md.currentTrack = random() % sizeof(tracks) / sizeof(tracks[0]);
    else {
        if (inc)
            md.currentTrack = (md.currentTrack + 1 == (sizeof(tracks) / sizeof(tracks[0]))) ? 0 : md.currentTrack + 1;
        else
            md.currentTrack = md.currentTrack == 0 ? (sizeof(tracks) / sizeof(tracks[0])) - 1 : md.currentTrack - 1;
    }

    const char* track = (const char*)tracks[md.currentTrack];

    *music
        = LoadMusicStream(track);
    musicLoaded = true;

    music->looping = true;
    SetMusicVolume(*music, md.currentVolume);

    AttachAudioStreamProcessor(music->stream, DataGrabber);

    if (md.title != NULL)
        free(md.title);
    md.title = trimTitle(track);
    md.rate = music->stream.sampleRate;
    md.size = music->stream.sampleSize;
    md.channels = music->stream.channels;

    PlayMusicStream(*music);
}

void DrawVolumeBar()
{
    int x = SCREEN_WIDTH / 2 - VOLUME_WIDTH / 2;
    int y = SCREEN_HEIGHT / 2 - VOLUME_HEIGHT / 2;

    int w = VOLUME_WIDTH * md.currentVolume;

    DrawRectangle(x, y, VOLUME_WIDTH, VOLUME_HEIGHT, (Color) { 69, 69, 69, 255 });
    DrawRectangle(x + MARGIN / 2, y + MARGIN / 2, w - MARGIN, VOLUME_HEIGHT - MARGIN, (Color) { 69, 255, 69, 255 });

    char buf[BUF_SIZE];

    snprintf(buf, BUF_SIZE, "%d%%", (int)(md.currentVolume * 100));
    DrawText(buf, SCREEN_WIDTH / 2 - MeasureText(buf, VOLUME_TEXT_SIZE) / 2, SCREEN_HEIGHT / 2 - VOLUME_TEXT_SIZE / 2, VOLUME_TEXT_SIZE, (Color) { 255, 255, 255, 255 });
}

void ChangeVolume(Music* music, bool inc)
{
    if (inc) {
        md.currentVolume += VOLUME_STEP * GetFrameTime();
        if (md.currentVolume > 1)
            md.currentVolume = 1;
        SetMusicVolume(*music, md.currentVolume);
    } else {
        md.currentVolume -= VOLUME_STEP * GetFrameTime();
        if (md.currentVolume < 0)
            md.currentVolume = 0;
        SetMusicVolume(*music, md.currentVolume);
    }
}

void DrawBars()
{
    for (int i = 0; i < MUSIC_BAR_BANDS; i++) {
        int x = SCREEN_WIDTH / MUSIC_BAR_BANDS * i;
        int w = SCREEN_WIDTH / MUSIC_BAR_BANDS;
        int h = (SCREEN_HEIGHT - BAR_HEIGHT) * md.bands[i];
        int y = (SCREEN_HEIGHT - BAR_HEIGHT) - h;

        DrawRectangle(x, y, w, h, (Color) { 245, 169, 184, 100 });
    }
}

void DrawSong(Music* music)
{
    int elapsedSeconds = GetMusicTimePlayed(*music);
    int seconds = GetMusicTimeLength(*music);

    elapsedSeconds = elapsedSeconds % seconds;

    float percentage = ((float)elapsedSeconds / (float)seconds);

    int elapsedMinutes = elapsedSeconds / 60;
    elapsedSeconds = elapsedSeconds % 60;

    int minutes = seconds / 60;
    seconds = seconds % 60;

    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "%s - %02d:%02d / %02d:%02d", md.title, elapsedMinutes, elapsedSeconds, minutes,
        seconds);

    DrawRectangle(0, SCREEN_HEIGHT - BAR_HEIGHT, SCREEN_WIDTH, 32,
        (Color) { 69, 69, 69, 255 });

    DrawRectangle(0, SCREEN_HEIGHT - BAR_HEIGHT, SCREEN_WIDTH * percentage,
        BAR_HEIGHT, (Color) { 69, 255, 69, 255 });

    int textLength = MeasureText(buf, BAR_TEXT_SIZE);

    DrawText(buf, SCREEN_WIDTH - textLength - MARGIN,
        SCREEN_HEIGHT - BAR_HEIGHT + (BAR_HEIGHT - BAR_TEXT_SIZE) / 2,
        BAR_TEXT_SIZE, RAYWHITE);
}

void MoveBall(Ball* ball)
{
    ball->pos.x = ball->pos.x + (ball->dir.x * frameTime);
    ball->pos.y = ball->pos.y + (ball->dir.y * frameTime);

    if (ball->pos.x > SCREEN_WIDTH - ball->radius) {
        ball->pos.x = SCREEN_WIDTH - ball->radius;
        ball->dir.x = -ball->dir.x;
    } else if (ball->pos.x < ball->radius) {
        ball->pos.x = ball->radius;
        ball->dir.x = -ball->dir.x;
    }

    if (ball->pos.y > SCREEN_HEIGHT - ball->radius - BAR_HEIGHT) {
        ball->pos.y = SCREEN_HEIGHT - ball->radius - BAR_HEIGHT;
        ball->dir.y = -ball->dir.y;
    } else if (ball->pos.y < ball->radius) {
        ball->pos.y = ball->radius;
        ball->dir.y = -ball->dir.y;
    }
}

void DrawBall(Ball* ball)
{
    DrawCircle(ball->pos.x, ball->pos.y, ball->radius, ball->col);
}

void MovePenger(Penger* penger)
{
    penger->pos.x
        = penger->pos.x + ((penger->speed.x * frameTime) * (penger->flipped ? -1 : 1));

    if (penger->pos.x > SCREEN_WIDTH - penger->texture->width) {
        penger->pos.x = SCREEN_WIDTH - penger->texture->width;
        penger->flipped = !penger->flipped;
    } else if (penger->pos.x < 0) {
        penger->pos.x = 0;
        penger->flipped = !penger->flipped;
    }
}

void DrawPenger(Penger* penger)
{
    int w = penger->texture->width;
    int h = penger->texture->height;
    int nh = (3 * (h / 4)) + cos(GetTime() * 2 * 2.0 * M_PI) * h / 4;

    Rectangle source = { 0, 0, penger->flipped ? -w : w, h };
    Rectangle dest = { penger->pos.x, penger->pos.y + (h - nh), w, nh };

    DrawTexturePro(*(penger->texture), source, dest, (Vector2) { 0, 0 }, 0, RAYWHITE);
}

void DrawMyBackground()
{
    int cellSize = fmax(SCREEN_WIDTH / GRID_COLS, SCREEN_HEIGHT / GRID_COLS);

    for (int i = 0; i < GRID_COLS; i++) {
        for (int j = 0; j < GRID_COLS; j++) {
            DrawRectangleLines(1 + i * cellSize, j * cellSize,
                i + 1 >= GRID_COLS ? cellSize - 1 : cellSize, cellSize,
                (Color) { 69, 69, 69, 255 });
        }
    }
}

int main(void)
{
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec);

    for (int i = 0; i < sizeof(balls) / sizeof(Ball); i++) {
        balls[i] = (Ball) { .pos
            = { random() % SCREEN_WIDTH, random() % SCREEN_HEIGHT },
            .dir = { random() % 50 + 25, random() % 50 + 25 },
            .radius = random() % 34 + 35,
            .col = (Color) { random() % 255, random() % 255, random() % 255, 100 + random() % 125 } };
    }

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, TEXT);

    InitAudioDevice();

    Music music;
    md.currentVolume = 0.1;
    ChangeSong(&music, true, false);

    SetTargetFPS(TARGET_FPS);

    Image penger_img = LoadImage(PENGER_IMG);
    Texture2D penger_texture = LoadTextureFromImage(penger_img);
    UnloadImage(penger_img);

    Penger penger = (Penger) { .texture = &penger_texture,
        .pos = (Vector2) { 0, SCREEN_HEIGHT - penger_texture.height - BAR_HEIGHT },
        .speed = (Vector2) { 50, 0 },
        .flipped = false };

    while (!WindowShouldClose()) {
        frameTime = GetFrameTime();

        UpdateMusicStream(music);

        if (IsKeyPressed(KEY_SPACE))
            ChangeSong(&music, true, false);
        if (IsKeyPressed(KEY_D))
            ChangeSong(&music, false, true);
        if (IsKeyPressed(KEY_A))
            ChangeSong(&music, false, false);

        if (IsKeyDown(KEY_W))
            ChangeVolume(&music, true);
        if (IsKeyDown(KEY_S))
            ChangeVolume(&music, false);

        BeginDrawing();
        ClearBackground((Color) { 24, 24, 24, 255 });

        DrawMyBackground();
        DrawBars();

        for (int i = 0; i < sizeof(balls) / sizeof(Ball); i++) {
            MoveBall(&balls[i]);
            DrawBall(&balls[i]);
        }

        int textWidth = MeasureText(TEXT, TEXT_SIZE);
        double time = GetTime() * TEXT_SPEED * 2.0 * M_PI;
        DrawText(TEXT, SCREEN_WIDTH / 2 - textWidth / 2 + cos(time) * TEXT_ROTATE_X,
            SCREEN_HEIGHT / 2 - TEXT_SIZE / 2 + sin(time) * TEXT_ROTATE_Y,
            TEXT_SIZE, RAYWHITE);

        MovePenger(&penger);
        DrawPenger(&penger);
        DrawSong(&music);

        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_S))
            DrawVolumeBar();

        EndDrawing();
    }

    UnloadMusicStream(music);
    UnloadTexture(penger_texture);
    CloseAudioDevice();

    CloseWindow();

    return 0;
}
