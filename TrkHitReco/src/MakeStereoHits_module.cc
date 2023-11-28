//
// A module to create simple stereo hits out of StrawHits. StrawHit selection is done by flagging in an upstream module
//
//
//  Original Author: David Brown, LBNL
//

#include "canvas/Persistency/Common/Ptr.h"
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/Sequence.h"
#include "fhiclcpp/types/Table.h"

#include "Offline/GeometryService/inc/GeometryService.hh"
#include "Offline/GeometryService/inc/GeomHandle.hh"
#include "Offline/TrackerGeom/inc/Tracker.hh"
#include "Offline/RecoDataProducts/inc/StrawHit.hh"
#include "Offline/RecoDataProducts/inc/ComboHit.hh"
#include "Offline/RecoDataProducts/inc/StrawHitFlag.hh"
#include "Offline/TrkHitReco/inc/CombineStereoPoints.hh"
// boost
//#include <boost/accumulators/accumulators.hpp>
//#include <boost/accumulators/statistics/mean.hpp>
//#include <boost/accumulators/statistics/stats.hpp>
//#include <boost/accumulators/statistics/weighted_variance.hpp>
//#include <boost/accumulators/statistics/max.hpp>
//#include <boost/accumulators/statistics/min.hpp>
//using namespace boost::accumulators;
#include "TMath.h"

#include <iostream>
#include <limits>
#include <cfloat>
#include <list>

namespace mu2e {
  class MakeStereoHits : public art::EDProducer {
    public:
      struct Config {
        using Name    = fhicl::Name;
        using Comment = fhicl::Comment;
        fhicl::Atom<int>                 debug    { Name("DebugLevel"), Comment("Debug level")};
        fhicl::Atom<art::InputTag>       chTag    { Name("ComboHitCollection"),   Comment("Input ComboHit collection") };
        fhicl::Sequence<std::string>     shsel    { Name("StrawHitSelectionBits"),    Comment("Mask for selecting hits") };
        fhicl::Sequence<std::string>     shrej    { Name("StrawHitRejectionBits"),   Comment("Mask for rejecting hits") };
        fhicl::Atom<float>               maxDt    { Name("MaxDt"),   Comment("Maximum time separation (ns)") };
        fhicl::Atom<float>               maxDPerp { Name("MaxDPerp"),   Comment("maximum perpendicular distance (mm)") };
        fhicl::Atom<float>               maxwdot  { Name("MaxWdot"),   Comment("maximum cos of angle between straws") };
        fhicl::Atom<float>               maxChisq { Name("MaxChisquared"),   Comment("position matching") };
        fhicl::Atom<float>               uvres    { Name("UVRes"),   Comment("Resolution in U,V (X,Y) plane") };
        fhicl::Atom<float>               minRho   { Name("MinRho"),   Comment("Minimum transverse radius of combination (mm)") };
        fhicl::Atom<float>               maxRho   { Name("MaxRho"),   Comment("Maximum transverse radius of combination (mm)") };
        fhicl::Atom<unsigned>            maxfsep  { Name("MaxFaceSeparation"),   Comment("max separation between faces") };
        fhicl::Atom<float>               maxDz    { Name("MaxDz"),   Comment("max Z separation between panels (mm)") };
        fhicl::Atom<bool>                testflag { Name("TestFlag"),   Comment("Test input hit flags") };
        fhicl::Atom<bool>                filter  { Name("FilterHits"),            Comment("Filter hits (alternative is to just flag)") };
        fhicl::Atom<std::string>         smask    { Name("SelectionMask"),   Comment("define the mask to select hits") };
      };

      explicit MakeStereoHits(const art::EDProducer::Table<Config>& config);
      void produce( art::Event& e);
      virtual void beginJob();
      virtual void beginRun(art::Run & run);
    private:
      typedef std::vector<uint16_t> ComboHits;

      int            _debug;
      art::InputTag _chTag;

      StrawHitFlag   _shsel;     // input flag selection
      StrawHitFlag   _shrej;     // input flag rejection
      float         _maxDt;      // maximum time separation between hits
      float         _maxDPerp;   // maximum transverse separation
      float         _maxwdot;    // minimum dot product of straw directions
      float         _maxChisq;   // maximum chisquared to allow making stereo hits
      float         _uvvar;      // intrinsic variance in UV plane
      float  _minrho, _maxrho;   // transverse radius range
      unsigned      _maxfsep;    // max face separation
      float         _maxDz;      // max z sepration
      bool          _testflag;   // test the flag or not
      bool          _filter;
      StrawIdMask   _smask;      // mask for combining hits

      std::array<std::vector<StrawId>,StrawId::_nupanels > _panelOverlap;   // which panels overlap each other
      void genMap();
      void fillComboHit(ComboHit& ch, CombineStereoPoints const& cpts, ComboHitCollection const& inchcol) const;
  };

  MakeStereoHits::MakeStereoHits(const art::EDProducer::Table<Config>& config) :
    art::EDProducer{config},
    _debug(config().debug()),
    _chTag(config().chTag()),
    _shsel(config().shsel()),
    _shrej(config().shrej()),
    _maxDt(config().maxDt()),
    _maxDPerp(config().maxDPerp()),
    _maxwdot(config().maxwdot()),
    _maxChisq(config().maxChisq()),
    _uvvar(config().uvres()*config().uvres()),
    _minrho(config().minRho()),
    _maxrho(config().maxRho()),
    _maxfsep(config().maxfsep()),
    _maxDz(config().maxDz()),
    _testflag(config().testflag()),
    _filter(config().filter()),
    _smask(config().smask())
    {
      consumes<ComboHitCollection>(_chTag);
      produces<ComboHitCollection>();
    }

  void MakeStereoHits::beginJob() {
  }

  void MakeStereoHits::beginRun(art::Run & run) {
    genMap();
  }

  void MakeStereoHits::produce(art::Event& event) {
    // I have to get a Handle, not a ValidHandle, as a literal handle is needed to find the productID
    art::Handle<ComboHitCollection> chH;
    if(!event.getByLabel(_chTag, chH))
      throw cet::exception("RECO")<<"mu2e::MakeStereoHits: No ComboHit collection found for tag" <<  _chTag << std::endl;
    auto const& inchcol = *chH.product();
    auto chcol = std::make_unique<ComboHitCollection>();
    chcol->reserve(inchcol.size());
    chcol->setParent(chH);
    // sort hits by unique panel.  This should be built in by construction upstream TODO!!
    std::array<std::vector<uint16_t>,StrawId::_nupanels> phits;
    size_t nch = inchcol.size();
    if(_debug > 2)std::cout << "MakeStereoHits found " << nch << " Input hits" << std::endl;
    std::vector<bool> used(nch,false);
    for(uint16_t ihit=0;ihit<nch;++ihit){
      ComboHit const& ch = inchcol[ihit];
      // select hits based on flag
      if( (!_testflag) ||( ch.flag().hasAllProperties(_shsel) && (!ch.flag().hasAnyProperty(_shrej))) ){
        phits[ch.strawId().uniquePanel()].push_back(ihit);
      }
    }
    if(_debug > 3){
      for (unsigned ipan=0; ipan < StrawId::_nupanels; ++ipan) {
        if(phits[ipan].size() > 0 ){
          std::cout << "Panel " << ipan << " has " << phits[ipan].size() << " hits "<< std::endl;
        }
      }
    }
    //  Loop over all hits.  Every one must appear somewhere in the output
    for (size_t ihit=0;ihit<nch;++ihit) {
      if(used[ihit])continue;
      used[ihit] = true;
      ComboHit const& ch1 = inchcol[ihit];
      // initialize new hit
      ComboHit combohit;
      combohit._sid = _smask.maskStrawId(ch1.strawId());
      // create a combiner seeded on this hit
      CombineStereoPoints cpts(_uvvar);
      cpts.addPoint(StereoPoint(ch1.pos(),ch1.uDir(),ch1.uVar(),ch1.vVar()),ihit);
      if( (!_testflag) ||( ch1.flag().hasAllProperties(_shsel) && (!ch1.flag().hasAnyProperty(_shrej))) ){
        // loop over the panels which overlap this hit's panel
        for (auto sid : _panelOverlap[ch1.strawId().uniquePanel()]) {
          // loop over hits in the overlapping panel
          for (auto jhit : phits[sid.uniquePanel()]) {
            const ComboHit& ch2 = inchcol[jhit];
            if(_debug > 3) std::cout << " comparing hits in panels " << ch1.strawId().uniquePanel() << " and " << ch2.strawId().uniquePanel() << std::endl;
            if (!used[jhit] && cpts.nPoints() < ComboHit::MaxNCombo){
              float dt;
              dt = fabs(ch1.correctedTime()-ch2.correctedTime());
              if(_debug > 4) std::cout << " dt = " << dt;
              if (dt < _maxDt){
                XYZVectorF dp = ch1.pos()-ch2.pos();
                auto dperp = dp.Rho();
                // negative crosings are in opposite quadrants and longitudinal separation isn't too big
                if(_debug > 4) std::cout << " dperp = " << dperp;
                if (dperp < _maxDPerp ) {
                  // compute chisq WRT this point
                  StereoPoint twodpt(ch2.pos(),ch2.uDir(),ch2.uVar(),ch2.vVar());
                  double dchi2 = cpts.dChi2(twodpt);
                  if(_debug > 3) std::cout << " dchisq = " << dchi2;
                  if(dchi2 < _maxChisq){
                    // provisionally add this hit
                    cpts.addPoint(twodpt,jhit);
                    auto rho = cpts.point().point().pos2().R();
                    if(_debug > 4) std::cout << " rho = " << rho << std::endl;
                    if(rho < _maxrho && rho > _minrho ){
                      if(_debug > 3) std::cout << " added index";
                      used[jhit] = true;
                    } else {
                      // remove the point
                      cpts.removePoint(jhit);
                    }
                  }
                }
              }
            }
            if(_debug > 3) std::cout << std::endl;
          }
        }
      }
      if(cpts.nPoints() >1){
        // create the combined hit
        fillComboHit(combohit,cpts,inchcol);
      } else {
        // reference the initial hit
        combohit.init(ch1,ihit);
        // test radius
        auto rho = combohit.pos().Rho();
        if(rho < _maxrho && rho > _minrho )
          combohit._flag.merge(StrawHitFlag::radsel);
        else
          combohit._flag.clear(StrawHitFlag::radsel);
      }
      // update the level
      combohit._mask = _smask;
      // final test
      if( (!_filter) || ( combohit.flag().hasAllProperties(_shsel) && (!combohit.flag().hasAnyProperty(_shrej))) )
        chcol->push_back(combohit);
    }
    event.put(std::move(chcol));
  }

  void MakeStereoHits::fillComboHit(ComboHit& combohit, CombineStereoPoints const& cpts, ComboHitCollection const& inchcol) const {
    if(_debug > 1){
      std::cout << "Combining " << cpts.nPoints() << " hits" << std::endl;
    }
    // solve for the line
    StereoLine sline;
    if(cpts.stereoLine(sline)){
      combohit._pos = sline.pos(sline.z0());
      // create a 2-D point from the upper component of this
      TwoDPoint spt(sline.pars().Sub<TwoDPoint::SVEC>(0), sline.cov().Sub<TwoDPoint::SMAT>(0,0));
      auto ud2 =  spt.udir();
      auto vd2 =  spt.vdir();
      combohit._udir = XYZVectorF(ud2.X(),ud2.Y(),0.0);
      combohit._vdir = XYZVectorF(vd2.X(),vd2.Y(),0.0);
      combohit._ures = sqrt(spt.uvar());
      combohit._vres = sqrt(spt.vvar());
      // extract the slopes from the stereo line and turn them into a direction
      combohit._hdir = sline.dir();
      TwoDPoint::SVEC ud(ud2.X(),ud2.Y());
      TwoDPoint::SVEC vd(vd2.X(),vd2.Y());
      TwoDPoint::SMAT dmat = sline.cov().Sub<TwoDPoint::SMAT>(2,2);
      double uzvar = ROOT::Math::Similarity(ud,dmat);
      double vzvar = ROOT::Math::Similarity(vd,dmat);
      combohit._uzres = sqrt(uzvar);
      combohit._vzres = sqrt(vzvar);
      // fit quality
      combohit._qual = TMath::Prob(sline.chisq(),sline.ndof());
    }  else {
      // fall back to the 2D projection
      // fill position and variance from combined info
      auto const& pt = cpts.point();
      combohit._pos = pt.pos3();
      auto udir = pt.udir();
      auto vdir = pt.vdir();
      combohit._udir = XYZVectorF(udir.X(),udir.Y(),0.0);
      combohit._vdir = XYZVectorF(vdir.X(),vdir.Y(),0.0);
      combohit._ures = sqrt(pt.uvar());
      combohit._vres = sqrt(pt.vvar());
      combohit._qual = cpts.consistency();
    }

    // fill the remaining variables
    double twtsum(0), zmin(std::numeric_limits<float>::max()), zmax(std::numeric_limits<float>::lowest());
    for(auto iwt = cpts.weights().begin(); iwt != cpts.weights().end(); ++iwt){
      auto ihit = iwt->first;
      if(combohit.addIndex(ihit)) {
        auto const& ch = inchcol[ihit];
        unsigned nsh = ch.nStrawHits();
        combohit._nsh += nsh;
        combohit._flag.merge(ch.flag());
        double z = ch._pos.Z();
        zmin = std::min(zmin,z);
        zmax = std::max(zmax,z);
        combohit._flag.merge(StrawHitFlag::stereo);
        combohit._edep += ch.energyDep()*nsh;
        // the following have unclear meaning for stereo hits, but we fill them anyways
        double twt = 1.0/ch.timeVar();
        for(size_t iend=0;iend<2;++iend){
          combohit._etime[iend] += ch._etime[iend]*twt;
          combohit._tot[iend] += ch._tot[iend]*twt;
        }
        twtsum += twt;
        combohit._time += ch.correctedTime()*twt;
        // to remove
        combohit._dtime += ch._dtime*twt;
        combohit._ptime += ch._ptime*twt;
      } else {
        std::cout << "MakeStereoHits past limit" << std::endl;
      }
    }
    // define w error from range
    static const double invsqrt12 = 1.0/sqrt(12.0);
    combohit._wres = invsqrt12*(zmax-zmin);
    // simple average for edep
    combohit._edep /= combohit._nsh;
    // average time
    combohit._time /= twtsum;
    combohit._dtime /= twtsum;
    combohit._ptime  /= twtsum;
    combohit._timeres = sqrt(1.0/twtsum);
    for(size_t iend=0;iend<2;++iend){
      combohit._etime[iend] /=twtsum;
      combohit._tot[iend] /= twtsum;
    }
  }

  // generate the overlap map
  void MakeStereoHits::genMap() {
    static bool init(false);
    if(!init){
      init = true;
      // initialize
      const Tracker& tt(*GeomHandle<Tracker>());
      // establihit the extent of a panel using the longest straw (0)
      Straw const& straw = tt.getStraw(StrawId(0,0,0));
      float phi0 = (straw.getMidPoint()-straw.halfLength()*straw.getDirection()).phi();
      float phi1 = (straw.getMidPoint()+straw.halfLength()*straw.getDirection()).phi();
      float lophi = std::min(phi0,phi1);
      float hiphi = std::max(phi0,phi1);
      float phiwidth = hiphi-lophi;
      if (phiwidth>M_PI) phiwidth = 2*M_PI-phiwidth;
      if(_debug > 1)std::cout << "Panel Phi width = " << phiwidth << std::endl;
      // loop over all unique panels
      for(size_t ipla = 0;ipla < StrawId::_nplanes; ++ipla) {
        for(int ipan=0;ipan<StrawId::_npanels;++ipan){
          StrawId sid(ipla,ipan,0);
          uint16_t upan = sid.uniquePanel();
          Straw const& straw = tt.getStraw(StrawId(ipla,ipan,0));
          float phi = straw.getMidPoint().phi();
          if(_debug > 1)std::cout << "Plane " << ipla << " Panel " << ipan << " phi = " << phi << " z = " << straw.getMidPoint().z() << std::endl;
          // loop over nearby panels and check for an overlap
          for(size_t jpla = 0;jpla < StrawId::_nplanes; ++jpla) {
            for(int jpan=0;jpan<StrawId::_npanels;++jpan){
              StrawId osid(jpla,jpan,0);
              if(osid != sid && _smask.equal(osid,sid) && (unsigned)abs(osid.uniqueFace() - sid.uniqueFace()) <= _maxfsep ) {
                Straw const& ostraw = tt.getStraw(StrawId(jpla,jpan,0));
                float dphi = fabs(fmod(phi - ostraw.getMidPoint().phi(),2*M_PI));
                if (dphi > M_PI) dphi = 2*M_PI-dphi;
                if(_debug > 1)std::cout << "Test Plane " << jpla << " Panel " << jpan << " dphi " << dphi << std::endl;
                if (dphi < phiwidth){
                  // insure the straws aren't parallel and are close enough in Z
                  double wdot = fabs(straw.direction().dot(ostraw.direction()));
                  double dz = fabs((straw.origin()-ostraw.origin()).z());
                  if(_debug > 1)std::cout << "Dz " << dz << " wdot " << wdot << std::endl;
                  if(wdot < _maxwdot && dz < _maxDz ){
                    if(_debug > 1)std::cout << "Added overlapping panel " << std::endl;
                    _panelOverlap[upan].push_back(osid);
                  }
                }
              }
            }
          }
        }
      }
      if (_debug >0) {
        for(uint16_t ipan = 0; ipan < StrawId::_nupanels; ++ipan) {
          std::cout << "Unique Panel " << ipan << " Overlaps with the panels: ";
          for(auto sid : _panelOverlap[ipan])
            std::cout << sid.uniquePanel() << ", ";
          std::cout << std::endl;
        }
      }
    }
  }
}

using mu2e::MakeStereoHits;
DEFINE_ART_MODULE(MakeStereoHits)

