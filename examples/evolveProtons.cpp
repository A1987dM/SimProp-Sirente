#include <algorithm>

#include "simprop.h"

using namespace simprop;

auto IsActive = [](const Particle& p) {
  const double minPropagatingGamma = 1e7;
  return (p.IsNucleus() && p.getRedshift() > 1e-20 && p.getGamma() > minPropagatingGamma);
};

class Evolutor {
 protected:
  const double deltaGammaCritical = 0.1;
  RandomNumberGenerator& m_rng;
  ParticleStack m_stack;
  std::shared_ptr<cosmo::Cosmology> m_cosmology;
  std::shared_ptr<photonfields::CMB> m_cmb;
  std::shared_ptr<photonfields::PhotonField> m_ebl;
  std::vector<std::shared_ptr<losses::ContinuousLosses> > m_continuousLosses;
  std::shared_ptr<interactions::PhotoPionProduction> m_pppcmb;

 public:
  Evolutor(RandomNumberGenerator& rng) : m_rng(rng) {
    m_cosmology = std::make_shared<cosmo::Planck2018>();
  }

  void buildParticleStack(Redshift z, LorentzFactor Gamma, size_t N = 1) {
    auto builder = SingleParticleBuilder(proton, N);
    builder.setRedshift(z.get());
    builder.setGamma(Gamma.get());
    m_stack = builder.build();
  }

  void buildPhotonFields() {
    m_cmb = std::make_shared<photonfields::CMB>();
    m_ebl = std::make_shared<photonfields::Dominguez2011PhotonField>();
  }

  void buildContinuousLosses() {
    std::vector<std::shared_ptr<photonfields::PhotonField> > phFields{m_cmb, m_ebl};
    m_continuousLosses = std::vector<std::shared_ptr<losses::ContinuousLosses> >{
        std::make_shared<losses::PairProductionLosses>(phFields),
        std::make_shared<losses::AdiabaticContinuousLosses>(m_cosmology)};
  }

  void buildStochasticInteractions() {
    auto sigma = std::make_shared<xsecs::PhotoPionProductionXsec>();
    m_pppcmb = std::make_shared<interactions::PhotoPionProduction>(sigma, m_cmb);
  }

  double computeStochasticRedshiftInterval(const Particle& particle, RandomNumber r) {
    const auto pid = particle.getPid();
    const auto zNow = particle.getRedshift();
    const auto Gamma = particle.getGamma();
    const auto dtdz = m_cosmology->dtdz(zNow);
    const auto lambda_s = std::fabs(1. / m_pppcmb->rate(pid, Gamma, zNow) / dtdz);
    // TODO why to put the fabs?
    return -lambda_s * std::log(1. - r.get());
  }

  double computeDeltaGamma(const Particle& particle, double dz) {
    const auto pid = particle.getPid();
    const auto zNow = particle.getRedshift();
    const auto Gamma = particle.getGamma();
    const auto dtdz = m_cosmology->dtdz(zNow);
    double dlnGammaNow = 0, dlnGammaHalf = 0, dlnGammaNext = 0;
    for (auto losses : m_continuousLosses) {
      dlnGammaNow += losses->dlnGamma_dt(pid, Gamma, zNow);
      auto halfRedshift = zNow - 0.5 * dz;
      dlnGammaHalf += losses->dlnGamma_dt(pid, Gamma, halfRedshift);
      auto nextRedhisft = zNow - dz;
      dlnGammaNext += losses->dlnGamma_dt(pid, Gamma, nextRedhisft);
    }
    return dz / 6. * dtdz * (dlnGammaNow + 4. * dlnGammaHalf + dlnGammaNext);
  }

  double computeLossesRedshiftInterval(const Particle& particle) {
    const auto zNow = particle.getRedshift();
    double dz = zNow;
    double deltaGamma = computeDeltaGamma(particle, zNow);
    if (deltaGamma > deltaGammaCritical) {
      dz = utils::rootFinder<double>(
          [&](double x) { return computeDeltaGamma(particle, x) - deltaGammaCritical; }, 0., zNow,
          100, 1e-5);
    }
    return dz;
  }

  void run(std::string filename) {
    utils::OutputFile out(filename.c_str());
    size_t nActive = std::count_if(m_stack.begin(), m_stack.end(), IsActive);
    while (nActive > 0) {
      const auto it = std::find_if(m_stack.begin(), m_stack.end(), IsActive);
      out << *it << " " << 0 << "\n";
      const auto nowRedshift = it->getRedshift();

      auto r = m_rng();
      const auto dz_s = computeStochasticRedshiftInterval(*it, RandomNumber(r));
      assert(dz_s > 0.);

      const auto dz_c = computeLossesRedshiftInterval(*it);
      assert(dz_c > 0. && dz_c <= nowRedshift);

      if (dz_s > dz_c || dz_s > nowRedshift) {
        const auto Gamma = it->getGamma();
        const auto dz = dz_c;
        const auto deltaGamma = computeDeltaGamma(*it, dz);
        it->getNow() = {nowRedshift - dz, Gamma * (1. - deltaGamma)};
      } else {
        const auto dz = dz_s;
        auto finalState = m_pppcmb->finalState(*it, nowRedshift - dz, m_rng);
        m_stack.erase(it);
        m_stack.insert(m_stack.begin(), finalState.begin(), finalState.end());
      }

      nActive = std::count_if(m_stack.begin(), m_stack.end(), IsActive);
    }
  }  // run()

  virtual ~Evolutor() = default;
};

int main() {
  try {
    utils::startup_information();
    {
      RandomNumberGenerator rng = utils::RNG<double>(10);
      utils::Timer timer("timer for Gamma = 1e10");
      Evolutor evolutor(rng);
      evolutor.buildParticleStack(Redshift(1.), LorentzFactor(1e12));
      evolutor.buildPhotonFields();
      evolutor.buildContinuousLosses();
      evolutor.buildStochasticInteractions();
      evolutor.run("test_proton_evolution_1_1e12_1.txt");
    }
    {
      RandomNumberGenerator rng = utils::RNG<double>(-23);
      utils::Timer timer("timer for Gamma = 1e10");
      Evolutor evolutor(rng);
      evolutor.buildParticleStack(Redshift(1.), LorentzFactor(1e12));
      evolutor.buildPhotonFields();
      evolutor.buildContinuousLosses();
      evolutor.buildStochasticInteractions();
      evolutor.run("test_proton_evolution_1_1e12_2.txt");
    }
    {
      RandomNumberGenerator rng = utils::RNG<double>(1000);
      utils::Timer timer("timer for Gamma = 1e10");
      Evolutor evolutor(rng);
      evolutor.buildParticleStack(Redshift(1.), LorentzFactor(1e12));
      evolutor.buildPhotonFields();
      evolutor.buildContinuousLosses();
      evolutor.buildStochasticInteractions();
      evolutor.run("test_proton_evolution_1_1e12_3.txt");
    }
    {
      RandomNumberGenerator rng = utils::RNG<double>(3);
      utils::Timer timer("timer for Gamma = 1e10");
      Evolutor evolutor(rng);
      evolutor.buildParticleStack(Redshift(1.), LorentzFactor(1e12), 100);
      evolutor.buildPhotonFields();
      evolutor.buildContinuousLosses();
      evolutor.buildStochasticInteractions();
      evolutor.run("test_proton_evolution_1_1e12_10.txt");
    }
    // {
    //   utils::Timer timer("timer for Gamma = 1e11");
    //   Evolutor evolutor(rng);
    //   evolutor.buildParticleStack(1., 1e12);
    //   evolutor.buildPhotonFields();
    //   evolutor.buildContinuousLosses();
    //   evolutor.buildStochasticInteractions();
    //   evolutor.run("test_proton_evolution_1e11.txt");
    // }
    // {
    //   utils::Timer timer("timer for Gamma = 1e12");
    //   Evolutor evolutor(rng);
    //   evolutor.buildParticleStack(1., 1e12);
    //   evolutor.buildPhotonFields();
    //   evolutor.buildContinuousLosses();
    //   evolutor.buildStochasticInteractions();
    //   evolutor.run("test_proton_evolution_1e12.txt");
    // }
  } catch (const std::exception& e) {
    LOGE << "exception caught with message: " << e.what();
  }
  return EXIT_SUCCESS;
}
