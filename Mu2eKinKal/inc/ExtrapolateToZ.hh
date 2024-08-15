// predicate for extrapolation to a given Z position in a given direction. Stops if the
// track changes direction.
#ifndef Mu2eKinKal_ExtrapolateToZ_hh
#define Mu2eKinKal_ExtrapolateToZ_hh
#include "Offline/Mu2eKinKal/inc/KKTrack.hh"
#include <limits>
#include "cetlib_except/exception.h"
namespace mu2e {
  class ExtrapolateToZ {
    public:
      ExtrapolateToZ() : maxDt_(-1.0), tol_(1e10),
      zmin_(std::numeric_limits<double>::max()),
      zmax_(-std::numeric_limits<double>::max()){}
      ExtrapolateToZ(double maxdt, double tol, double zmin, double zmax) :
        maxDt_(maxdt), tol_(tol), zmin_(zmin), zmax_(zmax) {
        if(zmin >= zmax) throw cet::exception("RECO")<<"Mu2eKinKal::ExtrapolateToZ: range configuration error"<< endl;
        }
      // interface for extrapolation
      double maxDt() const { return maxDt_; } // maximum time to extend the track, WRT the time of the first(last) measurement
      double tolerance() const { return tol_; } // tolerance on fractional momentum change
      // extrapolation predicate: the track will be extrapolated until this predicate returns false, subject to the maximum time
      template <class KTRAJ> bool needsExtrapolation(KinKal::Track<KTRAJ> const& ktrk, KinKal::TimeDir tdir, double time) const;
    private:
      double maxDt_; // maximum extrapolation time
      double tol_; // momentum tolerance in BField domain
      double zmin_; // minimum z value required
      double zmax_; // maximum z value required
  };

  template <class KTRAJ> bool ExtrapolateToZ::needsExtrapolation(KinKal::Track<KTRAJ> const& kktrk, double time) const {
    double zval = kktrk.fitTraj().position3(time).Z();
    double zdir = kktrk.fitTraj().direction(time).Z();
    double zdir0 = kktrk.fitTraj().direction(kktrk.fitTraj().t0()).Z();
    if(zdir*zdir0<0.0)return false; // if the track has reversed no need to extrapolate
    if(zdir > 0){ // downstream
      return tdir == TimeDir::forwards ? zval < zmax_ : zval > zmin_;
    } else { // upstream
      return tdir == TimeDir::forwards ? zval > zmin_ : zval < zmax_;
    }
  }
}
#endif
