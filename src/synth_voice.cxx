// SynthVoice factory implementation. See synth_voice.hxx.

#include "edm/synth_voice.hxx"

namespace edm {

std::unique_ptr<Voice> make_synth_voice(const SynthVoiceConfig& config, double sample_rate,
                                        Rng rng) {
  switch (config.wave) {
    case Wave::Sine:
      return std::make_unique<SynthVoice<Wave::Sine>>(config, sample_rate, rng);
    case Wave::Square:
      return std::make_unique<SynthVoice<Wave::Square>>(config, sample_rate, rng);
    case Wave::Saw:
      return std::make_unique<SynthVoice<Wave::Saw>>(config, sample_rate, rng);
    case Wave::Noise:
      return std::make_unique<SynthVoice<Wave::Noise>>(config, sample_rate, rng);
  }
  return nullptr;  // unreachable: Wave has exactly four values
}

} // namespace edm
