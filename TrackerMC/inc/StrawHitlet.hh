#ifndef TrackerMC_StrawHitlet_hh
#define TrackerMC_StrawHitlet_hh
//
// StrawHitlet represents charge arriving at one end of the straw from a single ionization.
// It does not include time folding
// or electronics effects.  It does include charge collection effects,
// such as avalanche fluctuations, trapping, gas quenching, and propagation
// effects such as drift time, wire propagation time delay, and dispersion
//
// $Id: StrawHitlet.hh,v 1.1 2013/12/07 19:51:42 brownd Exp $
// $Author: brownd $
// $Date: 2013/12/07 19:51:42 $
//
// Original author David Brown, LBNL
//

// C++ includes
#include <iostream>

// Mu2e includes
#include "DataProducts/inc/StrawIndex.hh"
#include "RecoDataProducts/inc/StrawEnd.hh"
#include "MCDataProducts/inc/StepPointMC.hh"
#include "art/Persistency/Common/Ptr.h"

namespace mu2e {
  class StrawHitlet{
    public:
      enum HitletType {unknown=-1,primary=0,xtalk=1,noise=2};
      // constructors
      StrawHitlet(); 
      StrawHitlet(const StrawHitlet&);
      // x-talk constructor
      explicit StrawHitlet(const StrawHitlet& primary, StrawIndex const& index, double xfactor);
      // ghost constructor
      explicit StrawHitlet(const StrawHitlet& primary, double deltat);
      StrawHitlet(HitletType type,
		  StrawIndex sindex,
		  StrawEnd end,
		  double time,
		  double charge,
		  double wiredist,
		  art::Ptr<StepPointMC> const& stepmc);

      StrawHitlet& operator = (StrawHitlet const& other);

      // Accessors
      HitletType type() const { return _type; }
      StrawIndex strawIndex() const { return _strawIndex; }
      StrawEnd strawEnd() const { return _end; }
      float   time()       const { return _time;}
      float   charge()  const { return _charge; }
      float   wireDistance() const { return _wdist; } 
      art::Ptr<StepPointMC> const&  stepPointMC() const { return _stepMC; }
      // Print contents of the object.
      void print( std::ostream& ost = std::cout, bool doEndl = true ) const;
    private:
      HitletType _type; // type of hitlet
      StrawIndex  _strawIndex;      // Straw index
      StrawEnd	_end;		  // which end of the straw
      float     _time;            // microbunch time at the wire end, in ns
      float     _charge;          // charge at the wire end, in units of pC
      float	_wdist;		  // distance along the wire the charge has traveled, used to calculate dispersion
      art::Ptr<StepPointMC>  _stepMC;	  // Ptr into StepPointMC collection
  };

} // namespace mu2e
#endif

