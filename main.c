#include "raylib.h"
#include "raymath.h"
#include <dirent.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#define MARGIN 16

#define TEXT "asdfghjkl"
#define TEXT_SIZE 69
#define TEXT_ROTATE_X SCREEN_WIDTH / 6
#define TEXT_ROTATE_Y SCREEN_HEIGHT / 4
#define TEXT_SPEED 0.25

#define PENGER_IMG "resources/penger.png"
#define PENGER_SPEED 500

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

#define MOUSE_RADIUS 500
#define BALL_PUSH 300

#define POPUP_DURATION 1

static unsigned int tracksLength = 0;
char** tracks = NULL;
static float popupDuration = 0;

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
            float newValue = 0;

            for (int j = i * samples_per_band; j < (i + 1) * samples_per_band; j = j + md.channels) {
                newValue += fabs((float)(samples[j] / 32768.0));
                if (md.channels == 2)
                    newValue += fabs((float)(samples[j + 1] / 32768.0));
            }

            newValue = (newValue / samples_per_band);

            md.bands[i] = Lerp(md.bands[i], newValue, 20.0 * frameTime);

            i++;
        }
    } else if (md.size == 32) {
        float* samples = (float*)buffer;

        unsigned int total_samples = frames * md.channels;

        unsigned int samples_per_band = total_samples / MUSIC_BAR_BANDS;

        int i = 0;
        while (i < MUSIC_BAR_BANDS) {
            float newValue = 0;
            for (int j = i * samples_per_band; j < (i + 1) * samples_per_band; j = j + md.channels) {
                newValue += fabs(samples[j]);
                if (md.channels == 2)
                    newValue += fabs(samples[j + 1]);
            }

            newValue = (newValue / samples_per_band);

            md.bands[i] = Lerp(md.bands[i], newValue, 20.0 * frameTime);

            i++;
        }
    } else {
        fprintf(stderr, "[-] unsupported md.size: %u\n", md.size);
    }

    return;
}

char* trimTitle(const char* title)
{
    if (title == NULL)
        return NULL;

    int startIndex = 0;
    int len = strlen(title);

    for (int i = 0; i < len; i++) {
        if (title[i] == '/' && i + 1 < len) {
            startIndex = i + 1;
        }
        if (title[i] == '.') {
            len = i;
            break;
        }
    }

    len -= startIndex;
    if (len <= 0)
        return NULL;

    char* newTitle = malloc(len + 1);
    if (newTitle == NULL)
        return NULL;

    strncpy(newTitle, title + startIndex, len);
    newTitle[len] = '\0';

    return newTitle;
}

/*
    Seeking is not supported in module formats
    https://github.com/raysan5/raudio/blob/711c86eae17db9a94af575f7a5b496244b48b22d/src/raudio.c#L1791C5-L1791C50
*/
typedef struct {
    bool seeking;
    float progress;
    float targetTime;
    float startTime;
    Music* music;
} SeekingState;

static SeekingState seekState = { 0 };

void StartSeeking(Music* music, float targetTime)
{
    seekState.seeking = true;
    seekState.targetTime = targetTime;
    seekState.startTime = GetMusicTimePlayed(*music);
    seekState.music = music;

    if (targetTime < GetMusicTimePlayed(*music)) {
        StopMusicStream(*music);
        PlayMusicStream(*music);

        seekState.startTime = 0;
    }

    SetMusicPitch(*music, 50.0);
    SetMasterVolume(0.0f);
    UpdateMusicStream(*music);
}

void UpdateSeeking()
{
    if (seekState.seeking) {
        seekState.progress = GetMusicTimePlayed(*(seekState.music)) / seekState.targetTime;
        SetMasterVolume(0.0f);

        if (seekState.targetTime > 0) {
            seekState.progress = GetMusicTimePlayed(*(seekState.music)) / seekState.targetTime;
        } else {
            seekState.progress = 1.0;
        }

        if (seekState.progress >= 1.0) {
            seekState.seeking = false;

            SetMusicPitch(*(seekState.music), 1.0);
            SetMasterVolume(md.currentVolume);
        }
    }
}

void ChangeSong(Music* music, bool rand, bool inc)
{
    if (seekState.seeking) {
        seekState.seeking = false;
        SetMusicPitch(*music, 1.0);
        SetMasterVolume(md.currentVolume);
    }

    if (musicLoaded) {
        UnloadMusicStream(*music);
        musicLoaded = false;
    }

    srand((int)(GetTime() * 1000));

    if (rand)
        md.currentTrack = random() % tracksLength;
    else {
        if (inc)
            md.currentTrack = (md.currentTrack + 1 == tracksLength) ? 0 : md.currentTrack + 1;
        else
            md.currentTrack = md.currentTrack == 0 ? tracksLength - 1 : md.currentTrack - 1;
    }

    const char* track = (const char*)tracks[md.currentTrack];

    *music
        = LoadMusicStream(track);
    musicLoaded = true;

    music->looping = true;
    SetMasterVolume(md.currentVolume);

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
    if (popupDuration <= 0)
        return;

    popupDuration -= frameTime;

    int x = SCREEN_WIDTH / 2 - VOLUME_WIDTH / 2;
    int y = SCREEN_HEIGHT / 2 - VOLUME_HEIGHT / 2;

    int w = Lerp(0, VOLUME_WIDTH, md.currentVolume);

    DrawRectangle(x, y, VOLUME_WIDTH, VOLUME_HEIGHT, (Color) { 69, 69, 69, 255 });
    DrawRectangle(x + MARGIN / 2, y + MARGIN / 2, w - MARGIN, VOLUME_HEIGHT - MARGIN, (Color) { 69, 255, 69, 255 });

    char buf[BUF_SIZE];

    snprintf(buf, BUF_SIZE, "%d%%", (int)(md.currentVolume * 100));
    DrawText(buf, SCREEN_WIDTH / 2 - MeasureText(buf, VOLUME_TEXT_SIZE) / 2, SCREEN_HEIGHT / 2 - VOLUME_TEXT_SIZE / 2, VOLUME_TEXT_SIZE, (Color) { 255, 255, 255, 255 });
}

void ChangeVolume(Music* music, bool inc)
{
    popupDuration = POPUP_DURATION;

    if (inc) {
        md.currentVolume += VOLUME_STEP * frameTime;
        if (md.currentVolume > 1)
            md.currentVolume = 1;
        SetMasterVolume(md.currentVolume);
    } else {
        md.currentVolume -= VOLUME_STEP * frameTime;
        if (md.currentVolume < 0)
            md.currentVolume = 0;
        SetMasterVolume(md.currentVolume);
    }
}

void DrawBars()
{
    for (int i = 0; i < MUSIC_BAR_BANDS; i++) {
        int x = SCREEN_WIDTH / MUSIC_BAR_BANDS * i;
        int w = SCREEN_WIDTH / MUSIC_BAR_BANDS;
        int h = (SCREEN_HEIGHT - BAR_HEIGHT) * (seekState.seeking ? 0.3 + (0.5 / (float)(random() % 8)) : md.bands[i]);
        int y = (SCREEN_HEIGHT - BAR_HEIGHT) - h;

        DrawRectangle(x, y, w, h, (Color) { (seekState.seeking) ? random() % 255 : 245, (seekState.seeking) ? random() % 255 : 169, (seekState.seeking) ? random() % 255 : 184, 100 });
    }
}

void DrawSong(Music* music)
{
    float elapsedSeconds = GetMusicTimePlayed(*music);
    float seconds = GetMusicTimeLength(*music);

    elapsedSeconds = fmod(elapsedSeconds, seconds);

    float percentage = ((float)elapsedSeconds / (float)seconds);

    int elapsedMinutes = elapsedSeconds / 60;
    elapsedSeconds = fmod(elapsedSeconds, 60);

    int minutes = seconds / 60;
    seconds = fmod(seconds, 60);

    char buf[BUF_SIZE];
    snprintf(buf, BUF_SIZE, "%s - %02d:%02.0f / %02d:%02.0f", md.title, elapsedMinutes, elapsedSeconds, minutes,
        seconds);

    DrawRectangle(0, SCREEN_HEIGHT - BAR_HEIGHT, SCREEN_WIDTH, 32,
        (Color) { 69, 69, 69, 255 });

    DrawRectangle(0, SCREEN_HEIGHT - BAR_HEIGHT, Lerp(0, SCREEN_WIDTH, percentage),
        BAR_HEIGHT, (Color) { 69, 255, 69, 255 });

    int textLength = MeasureText(buf, BAR_TEXT_SIZE);

    DrawText(buf, SCREEN_WIDTH - textLength - MARGIN,
        SCREEN_HEIGHT - BAR_HEIGHT + (BAR_HEIGHT - BAR_TEXT_SIZE) / 2,
        BAR_TEXT_SIZE, RAYWHITE);
}

void BallOutOfBounds(Ball* ball)
{
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

void MoveBall(Ball* ball)
{
    ball->pos.x = ball->pos.x + (ball->dir.x * frameTime);
    ball->pos.y = ball->pos.y + (ball->dir.y * frameTime);

    BallOutOfBounds(ball);
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

bool BallInMouseRadius(Vector2 mouse, Ball* ball)
{
    return (IsCursorOnScreen() && (ball->pos.x + ball->radius / 2 > mouse.x - MOUSE_RADIUS / 2 && ball->pos.x - ball->radius / 2 < mouse.x + MOUSE_RADIUS / 2 && ball->pos.y + ball->radius / 2 > mouse.y - MOUSE_RADIUS / 2 && ball->pos.y - ball->radius / 2 < mouse.y + MOUSE_RADIUS / 2));
}

void PushBall(Vector2 mouse, Ball* ball)
{
    Vector2 dist = Vector2Subtract(ball->pos, mouse);
    Vector2 normalize = Vector2Normalize(dist);

    ball->pos.x += (BALL_PUSH * frameTime) * normalize.x;
    ball->pos.y += (BALL_PUSH * frameTime) * normalize.y;

    ball->dir.x = fabs(ball->dir.x) * (normalize.x < 0 ? -1 : 1);
    ball->dir.y = fabs(ball->dir.x) * (normalize.y < 0 ? -1 : 1);

    BallOutOfBounds(ball);
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

void DrawTitleText()
{
    int textWidth = MeasureText(TEXT, TEXT_SIZE);
    double time = GetTime() * TEXT_SPEED * 2.0 * M_PI;

    int textLength = strlen(TEXT);
    int margin = 0;
    float normalize = (1.0 / (float)textLength) * 2.0 * M_PI;

    for (int i = 0; i < textLength; i++) {
        char c[2];
        c[0] = TEXT[i];
        c[1] = '\0';
        int cWidth = MeasureText(c, TEXT_SIZE);
        DrawText(c, SCREEN_WIDTH / 2 - textWidth / 2 + margin + cos(time) * TEXT_ROTATE_X, SCREEN_HEIGHT / 2 - TEXT_SIZE / 2 + sin(time + (i * normalize)) * TEXT_SIZE + sin(time) * TEXT_ROTATE_Y, TEXT_SIZE, RAYWHITE);
        margin += cWidth + (TEXT_SIZE / 10); // https://github.com/raysan5/raylib/blob/e00c5eb8b1068b1fb3c1c52fc00967749f2a990a/src/rtext.c#L1296C75-L1296C91
    }
}

bool CheckSuffix(const char* fileName, const char* suffix)
{
    return (strncmp(fileName + strlen(fileName) - sizeof(char) * strlen(suffix), suffix, strlen(suffix)) == 0);
}

void SearchForTracks()
{
    printf("[+] searching for tracks\n");

    const char* resourcesDir = "resources/";
    DIR* resources = opendir(resourcesDir);

    if (resources == NULL) {
        fprintf(stderr, "[-] Cannot open resources directory\n");
        return;
    }

    struct dirent* entity;
    while ((entity = readdir(resources)) != NULL) {
        if (CheckSuffix(entity->d_name, "xm") || CheckSuffix(entity->d_name, "mod") || CheckSuffix(entity->d_name, "XM") || CheckSuffix(entity->d_name, "MOD")) {

            tracks = realloc(tracks, (tracksLength + 1) * sizeof(char*));
            if (tracks == NULL) {
                fprintf(stderr, "[-] Memory allocation failed for tracks array\n");
                closedir(resources);
                return;
            }

            size_t pathLen = strlen(resourcesDir) + strlen(entity->d_name) + 1;
            tracks[tracksLength] = malloc(pathLen);
            if (tracks[tracksLength] == NULL) {
                fprintf(stderr, "[-] Memory allocation failed for track path\n");
                closedir(resources);
                return;
            }

            snprintf(tracks[tracksLength], pathLen, "%s%s", resourcesDir, entity->d_name);

            printf("[+] added track: %s (count: %u)\n", tracks[tracksLength], tracksLength + 1);
            tracksLength++;
        }
    }

    closedir(resources);
}

int main(void)
{
    SearchForTracks();

    srand(GetTime());

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
    SetMasterVolume(md.currentVolume);

    SetTargetFPS(TARGET_FPS);

    Image penger_img = LoadImage(PENGER_IMG);
    Texture2D penger_texture = LoadTextureFromImage(penger_img);
    UnloadImage(penger_img);

    Penger penger = (Penger) { .texture = &penger_texture,
        .pos = (Vector2) { 0, SCREEN_HEIGHT - penger_texture.height - BAR_HEIGHT },
        .speed = (Vector2) { PENGER_SPEED, 0 },
        .flipped = false };

    while (!WindowShouldClose()) {
        frameTime = GetFrameTime();

        UpdateMusicStream(music);

        if (IsKeyPressed(KEY_SPACE))
            ChangeSong(&music, true, false);
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
            ChangeSong(&music, false, true);
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
            ChangeSong(&music, false, false);

        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
            ChangeVolume(&music, true);
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
            ChangeVolume(&music, false);

        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mousePos.y <= SCREEN_HEIGHT && mousePos.y >= SCREEN_HEIGHT - BAR_HEIGHT) {
                float seekPos = (mousePos.x / SCREEN_WIDTH) * GetMusicTimeLength(music);

                StartSeeking(&music, seekPos);
            }
        }
        UpdateSeeking();

        BeginDrawing();
        ClearBackground((Color) { 24, 24, 24, 255 });

        DrawMyBackground();

        DrawBars();

        for (int i = 0; i < sizeof(balls) / sizeof(Ball); i++) {
            if (BallInMouseRadius(mousePos, &balls[i]))
                PushBall(mousePos, &balls[i]);
            else
                MoveBall(&balls[i]);

            DrawBall(&balls[i]);
        }

        DrawTitleText();

        MovePenger(&penger);
        DrawPenger(&penger);
        DrawSong(&music);

        DrawVolumeBar();

        EndDrawing();
    }

    UnloadMusicStream(music);
    UnloadTexture(penger_texture);
    CloseAudioDevice();

    CloseWindow();

    return 0;
}
