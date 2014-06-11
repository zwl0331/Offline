// A function to find the SimParticle that entered the volume
// (e.g. the gas in the straw) and is "responsible" for making a
// StepPointMC.  This aggregates showers inside a volume back to the
// particle that caused it.
//
// Andrei Gaponenko, 2014

#ifndef Mu2eUtilities_inc_particleEnteringG4Volume_hh
#define Mu2eUtilities_inc_particleEnteringG4Volume_hh

#include "art/Persistency/Common/Ptr.h"
#include "MCDataProducts/inc/SimParticle.hh"
#include "MCDataProducts/inc/StepPointMC.hh"

namespace mu2e{
  art::Ptr<mu2e::SimParticle> particleEnteringG4Volume(const StepPointMC& step);
}

#endif /* Mu2eUtilities_inc_particleEnteringG4Volume_hh */
