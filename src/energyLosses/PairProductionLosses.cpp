#include "simprop/energyLosses/PairProductionLosses.h"

#include <array>
#include <cmath>

#include "simprop/utils/logging.h"
#include "simprop/utils/numeric.h"

namespace simprop {
namespace losses {

double sum_c(double k) {
  static auto _c = std::array<double, 4>{0.8048, 0.1459, 1.137e-3, -3.879e-6};
  double value = 0;
  for (size_t i = 1; i <= 4; ++i) value += _c[i - 1] * std::pow(k - 2, (double)i);
  return value;
}

double sum_d(double k) {
  static auto _d = std::array<double, 4>{-86.07, 50.96, -14.45, 8 / 3.};
  auto lnk = std::log(k);
  double value = 0;
  for (size_t i = 0; i <= 3; ++i) value += _d[i] * std::pow(lnk, (double)i);
  return value;
}

double sum_f(double k) {
  static auto _f = std::array<double, 3>{2.910, 78.35, 1837};
  double value = 0;
  for (size_t i = 1; i <= 3; ++i) value += _f[i - 1] / std::pow(k, (double)i);
  return value;
}

double phi(double k) {
  if (k < 2.)
    return 0;
  else if (k < 25.) {
    return M_PI / 12. * pow4(k - 2) / (1. + sum_c(k));
  } else {
    return (k * sum_d(k)) / (1. - sum_f(k));
  }
}

PairProductionLosses::PairProductionLosses(const photonfields::PhotonFields& photonFields)
    : ContinuousLosses(), m_photonFields(photonFields) {
  LOGD << "calling " << __func__ << " constructor";
}

double PairProductionLosses::dotGamma(double Gamma) const {
  auto TwoGamma_mec2 = 2. * Gamma / SI::electronMassC2;
  double I = 0;
  for (auto phField : m_photonFields) {
    const auto epsmin = phField->getMinPhotonEnergy();
    const auto epsmax = phField->getMaxPhotonEnergy();
    const auto lkmin = std::log(TwoGamma_mec2 * epsmin);
    const auto lkmax = std::log(TwoGamma_mec2 * epsmax);
    I += utils::simpsonIntegration<double>(
        [TwoGamma_mec2, phField](double lnk) {
          auto k = std::exp(lnk);
          return phi(k) / k * phField->density(k / TwoGamma_mec2);
        },
        lkmin, lkmax, 200);
  }
  constexpr auto factor = SI::alpha * pow2(SI::electronRadius) * SI::cLight * SI::electronMassC2 *
                          (SI::electronMass / SI::protonMass);
  return factor * I / Gamma;
}

double PairProductionLosses::dlnGamma_dt(PID pid, double Gamma, double z) const {
  auto b_l = pow3(1. + z) * dotGamma(Gamma * (1. + z));  // TODO no EBL evolution?
  auto Z = (double)getPidNucleusCharge(pid);
  auto A = (double)getPidNucleusCharge(pid);
  b_l *= pow2(Z) / A;
  return std::max(b_l, 0.);
}

}  // namespace losses
}  // namespace simprop
