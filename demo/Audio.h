#pragma once

#include <soloud.h>
#include <soloud_wav.h>
#include <string>
#include <unordered_map>

class Engine;

// audio system
class Audio
{
  private:
    // engine parent
    Engine& engine;
    // audio engine
    SoLoud::Soloud soloud;
    // name => wave sound
    std::unordered_map<std::string, SoLoud::Wav> sounds;
    // sound queue
    SoLoud::Queue queue;

  public:
    // Construct the Audio object given the engine.
    Audio(Engine& engine);
    // Destruct the Audio object.
    ~Audio();
    // Load a sound.
    void load(const std::string& name, const std::string& extension = "mp3");
    // Play the specified sound.
    void play(const std::string& name);
    // Start the queue with the specified sound.
    void startQueue(const std::string& name);
    // Extend the queue with the specified sound.
    void extendQueue(const std::string& name);
    // Stop the queue.
    void stopQueue();
};