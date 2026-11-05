// Standalone diagnostic, not a production test target. See the audit report.
#include "core/Measurement.h"
#include "core/ModalAnalysisReportData.h"
#include "core/FiducialTransformFit.h"
#include "core/FringeOrdering.h"
#include "core/pipeline/stages/ModalAnalysisStage.h"
#include "core/psi/PhaseShifting.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
  using namespace digitqt::core;
  constexpr double pi = 3.14159265358979323846;
  // Identical 100 nm astigmatism rotated through 45 degrees.
  for (const auto basis : {PolynomialBasis::Seregin, PolynomialBasis::Zernike}) {
    for (const double angle : {0.0, pi / 4}) {
      Measurement m;
      m.polynomialBasis() = basis;
      m.setEdgeErosionPixels(0);
      m.wavefrontMap() = PhaseMap(101, 101);
      for (int y = 0; y < 101; ++y)
        for (int x = 0; x < 101; ++x) {
          const double u = (x - 50.0) / 45, v = -(y - 50.0) / 45;
          if (u*u + v*v <= 1)
            m.wavefrontMap().setValue(x, y, 100 * (std::cos(2*angle)*(u*u-v*v) +
                                                         std::sin(2*angle)*2*u*v));
        }
      pipeline::ModalAnalysisStage stage;
      const bool ok = stage.compute(m);
      const auto report = buildModalAnalysisReportData(m);
      std::cout << "astig basis=" << (basis == PolynomialBasis::Seregin ? "Seregin" : "Zernike")
                << " angle=" << angle*180/pi << " ok=" << ok
                << " report_magnitude_nm=" << report.astigMagnitude
                << " residual_rms_nm=" << m.modalAnalysis().rmsAfter << '\n';
    }
  }
  // Full-rank source triangle mapped to a collapsed target is not invertible.
  std::vector<Fiducial> fiducials(3);
  for (int i = 0; i < 3; ++i) {
    fiducials[i].id = i;
    fiducials[i].hasReference = true;
  }
  fiducials[1].imageX = 1;
  fiducials[2].imageY = 1;
  const auto fit = fitFiducialTransform(fiducials);
  std::cout << "collapsed_affine ok=" << fit.ok << " determinant="
            << fit.transform.a*fit.transform.e-fit.transform.b*fit.transform.d << '\n';

  // Unknown-shift model: compare uniform and spatially varying illumination.
  const std::vector<double> actual = {0, 72*pi/180, 160*pi/180, 248*pi/180, 325*pi/180};
  const std::vector<double> nominal = {0, pi/2, pi, 3*pi/2, 2*pi};
  const auto visible = [](int x, int y) { return (x-31.5)*(x-31.5)+(y-31.5)*(y-31.5)<29*29; };
  for (bool varying : {false, true}) {
    std::vector<Bitmap> frames;
    for (double shift : actual) {
      Bitmap frame(64,64);
      for (int y=0; y<64; ++y)
        for (int x=0; x<64; ++x) {
          const double u=(x-31.5)/29, v=(y-31.5)/29;
          const double phase=2*pi*(2*u+.7*(u*u+v*v));
          const double a=varying ? 90+50*std::exp(-2*((u-.4)*(u-.4)+v*v)) : 120;
          const double b=varying ? 25+65*std::exp(-3*((u+.3)*(u+.3)+v*v)) : 80;
          frame.setPixel(x,y,static_cast<uint8_t>(std::lround(a+b*std::cos(phase+shift))));
        }
      frames.push_back(std::move(frame));
    }
    for (bool known : {true,false}) {
      psi::PsiResult result;
      std::string error;
      const bool ok=known ? psi::solveWithKnownShifts(frames,actual,visible,{},result,error)
                         : psi::solveWithUnknownShifts(frames,nominal,visible,{},result,error);
      double maxShiftError=0, sum=0;
      int count=0;
      if (ok) {
        for (size_t i=0;i<actual.size();++i)
          maxShiftError=std::max(maxShiftError,std::abs(std::remainder(result.shifts[i]-actual[i],2*pi))*180/pi);
        for(int y=0;y<64;++y) for(int x=0;x<64;++x) if(visible(x,y)) {
          const double u=(x-31.5)/29,v=(y-31.5)/29;
          const double error=std::remainder(result.wrappedPhase.value(x,y)-2*pi*(2*u+.7*(u*u+v*v)),2*pi);
          sum+=error*error;
          ++count;
        }
      }
      std::cout << "psi varying=" << varying << " known=" << known << " ok=" << ok
                << " converged=" << result.converged << " shift_max_error_deg=" << maxShiftError
                << " phase_rms_orders=" << (count ? std::sqrt(sum/count)/(2*pi) : -1)
                << " residual=" << result.diagnostics.residual << " error=" << error << '\n';
    }
  }
}
