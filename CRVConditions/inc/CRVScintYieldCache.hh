#ifndef CRVConditions_CRVScintYieldCache_hh
#define CRVConditions_CRVScintYieldCache_hh

#include "Offline/CRVConditions/inc/CRVScintYieldMaker.hh"
#include "Offline/Mu2eInterfaces/inc/ProditionsCache.hh"

namespace mu2e {

class CRVScintYieldCache : public ProditionsCache {
 public:
  CRVScintYieldCache(CRVScintYieldConfig const& config) :
      ProditionsCache(CRVScintYield::cxname, config.verbose()),
      _useDb(config.useDb()), _maker(config) {}

  void initialize() {
    if (_useDb) {
      _sci_p = std::make_unique<DbHandle<CRVScint>>();
    }
  }

  set_t makeSet(art::EventID const& eid) {
    ProditionsEntity::set_t cids;
    if (_useDb) {
      _sci_p->get(eid);
      cids.insert(_sci_p->cid());
    }
    return cids;
  }

  DbIoV makeIov(art::EventID const& eid) {
    DbIoV iov;
    iov.setMax();
    if (_useDb) {
      _sci_p->get(eid);
      iov.overlap(_sci_p->iov());
    }
    return iov;
  }

  ProditionsEntity::ptr makeEntity(art::EventID const& eid) {
    if (_useDb) {
      return _maker.fromDb(_sci_p->getPtr(eid));
    } else {
      return _maker.fromFcl();
    }
  }

 private:
  bool _useDb;
  CRVScintYieldMaker _maker;

  // these handles are not default constructed
  // so the db can be completely turned off
  std::unique_ptr<DbHandle<CRVScint>> _sci_p;
};

}  // namespace mu2e

#endif
