#include "Audio.h"

#include "Engine.h"
#include <thread>

using namespace SoLoud;

Audio::Audio(Engine& engine)
    : engine(engine)
{
    soloud.init();
    load("intro");
    load("main");
    load("finale");
    load("credits");
}

Audio::~Audio()
{
    soloud.fadeGlobalVolume(0.0f, 0.4f);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    soloud.deinit();
}

void Audio::load(const std::string& name, const std::string& extension)
{
    const std::string path = soundPath(name, extension).string();
    sounds[name] = Wav();
    sounds[name].load(path.data());
}

void Audio::play(const std::string& name)
{
    Wav& sound = sounds[name];
    soloud.play(sound);
}

void Audio::startQueue(const std::string& name)
{
    Wav& sound = sounds[name];
    queue.setParamsFromAudioSource(sound);
    soloud.play(queue);
    queue.play(sound);
}

void Audio::extendQueue(const std::string& name)
{
    Wav& sound = sounds[name];
    queue.play(sound);
}

void Audio::stopQueue()
{
    soloud.fadeVolume(queue.mQueueHandle, 0.0f, 1.0f);
    soloud.scheduleStop(queue.mQueueHandle, 1.0f);
}