#ifndef SIMPROP_XSECS_PHOTOPIONPRODUCTION_H
#define SIMPROP_XSECS_PHOTOPIONPRODUCTION_H

#include <string>
#include <vector>

#include "simprop/crossSections/CrossSection.h"
#include "simprop/utils/lookupContainers.h"

namespace simprop {
namespace xsecs {

class PhotoPionProduction final : public CrossSection {
 protected:
  const std::string m_filename = "data/xsec_ppp.txt";
  utils::LookupArray<1000> m_sigmas{m_filename};

 public:
  PhotoPionProduction() {}
  virtual ~PhotoPionProduction() = default;
  double get(PID pid, double photonEnergy) const override;
};

}  // namespace xsecs
}  // namespace simprop

#endif