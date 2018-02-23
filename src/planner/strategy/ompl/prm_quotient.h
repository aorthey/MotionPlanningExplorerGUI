#pragma once
#include "prm_basic.h"
#include <ompl/datastructures/PDF.h>

namespace ob = ompl::base;
namespace og = ompl::geometric;


namespace ompl
{
  namespace base
  {
      OMPL_CLASS_FORWARD(OptimizationObjective);
  }
  namespace geometric
  {
    class PRMQuotient: public og::PRMBasic{

      public:

        PRMQuotient(const ob::SpaceInformationPtr &si, Quotient *previous_);
        virtual ~PRMQuotient() override;

      protected:
        double epsilon{0.05}; //graph thickening

        virtual bool SampleGraph(ob::State*) override;
        virtual ompl::PDF<og::PRMBasic::Edge> GetEdgePDF();

    };

  };
};