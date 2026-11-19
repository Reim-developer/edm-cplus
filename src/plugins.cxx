// plugins implementation. Slide<W> is a template, so its
// definitions live in plugins.hxx by necessity (like
// Oscillator<Wave>); this file explicitly instantiates the four
// Wave specializations so the library ships them.

#include "edm/plugins.hxx"

namespace edm::plugins {

template class Slide<Wave::Sine>;
template class Slide<Wave::Square>;
template class Slide<Wave::Saw>;
template class Slide<Wave::Noise>;

} // namespace edm::plugins
