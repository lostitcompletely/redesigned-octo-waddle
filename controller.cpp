#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <cstring>
#include <chrono>
#include <portaudio.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <unistd.h>
#include <termios.h>

int _kbhit() {
    termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);

    int hit = FD_ISSET(STDIN_FILENO, &fds);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return hit;
}

int _getch() {
    termios oldt, newt;
    int ch;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return ch;
}
#endif

#include "effects/phaser.h"
#include "effects/exciter.h"
#include "effects/reverb.h"

// ------------------ EFFECT INSTANCES ------------------
static Phaser  gPhaser;
static Exciter gExciter;
static Reverb  gReverb;

static std::atomic<bool> usePhaser(false);
static std::atomic<bool> useExciter(false);
static std::atomic<bool> useReverb(false);

// ------------------ Audio Callback ---------------------
static int audioCallback(const void* input, void* output,
                         unsigned long frames,
                         const PaStreamCallbackTimeInfo*,
                         PaStreamCallbackFlags,
                         void*)
{
    const float* in  = (const float*)input;
    float* out = (float*)output;

    if (!in) {
        memset(out, 0, frames * 2 * sizeof(float));
        return paContinue;
    }

    for (unsigned long i = 0; i < frames; ++i) {
        float x = in[i];

        if (usePhaser)  x = gPhaser.process(x);
        if (useExciter) x = gExciter.process(x);
        if (useReverb)  x = gReverb.process(x);

        out[2*i + 0] = x;
        out[2*i + 1] = x;
    }

    return paContinue;
}

// ------------------ MAIN -------------------------------
int main() {
    Pa_Initialize();

    int devCount = Pa_GetDeviceCount();
    if (devCount < 0) {
        std::cerr << "Pa_GetDeviceCount failed\n";
        return 1;
    }

    std::cout << "Available audio devices:\n";
    for (int i = 0; i < devCount; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        const PaHostApiInfo* host = Pa_GetHostApiInfo(info->hostApi);
        std::cout << "[" << i << "] " << info->name
                  << " (host: " << host->name << ")"
                  << " IN:" << info->maxInputChannels
                  << " OUT:" << info->maxOutputChannels << "\n";
    }

    int inputIndex = Pa_GetDefaultInputDevice();
    std::cout << "\nEnter input device index (default "
              << inputIndex << "): ";
    std::cin >> inputIndex;

    if (inputIndex < 0 || inputIndex >= devCount) {
        std::cerr << "Invalid index.\n";
        return 1;
    }

    // Prepare effects
    int sampleRate = 48000;
    gPhaser.prepare(sampleRate);
    gExciter.prepare(sampleRate);
    gReverb.prepare(sampleRate);

    // Stream params
    PaStreamParameters inP{}, outP{};
    inP.device = inputIndex;
    inP.channelCount = 1;
    inP.sampleFormat = paFloat32;
    inP.suggestedLatency =
        Pa_GetDeviceInfo(inputIndex)->defaultLowInputLatency;

    outP.device = Pa_GetDefaultOutputDevice();
    outP.channelCount = 2;
    outP.sampleFormat = paFloat32;
    outP.suggestedLatency =
        Pa_GetDeviceInfo(outP.device)->defaultLowOutputLatency;

    PaStream* stream;
    Pa_OpenStream(&stream, &inP, &outP,
                  sampleRate, 256,
                  paClipOff, audioCallback, nullptr);

    Pa_StartStream(stream);

    std::cout << "\n--- Guitar Effects Controller ---\n";
    std::cout << "Press:\n"
              << "  1 = Toggle Phaser\n"
              << "  2 = Toggle Exciter\n"
              << "  3 = Toggle Reverb\n"
              << "  q = Quit\n\n";

    bool running = true;
    while (running) {
        if (_kbhit()) {
            char c = _getch();
            switch (c) {
                case '1':
                    usePhaser = !usePhaser;
                    std::cout << "Phaser:  " << (usePhaser ? "ON" : "OFF") << "\n";
                    break;

                case '2':
                    useExciter = !useExciter;
                    std::cout << "Exciter: " << (useExciter ? "ON" : "OFF") << "\n";
                    break;

                case '3':
                    useReverb = !useReverb;
                    std::cout << "Reverb:  " << (useReverb ? "ON" : "OFF") << "\n";
                    break;

                case 'q':
                    running = false;
                    break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
