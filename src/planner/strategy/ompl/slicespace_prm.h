#pragma once

#include "slicespace.h"
#include "planner/cspace/cspace_factory.h"

#include <ompl/geometric/planners/PlannerIncludes.h>
#include <ompl/datastructures/NearestNeighbors.h>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/pending/disjoint_sets.hpp>
#include <utility>
#include <vector>
#include <map>

namespace ob = ompl::base;
namespace og = ompl::geometric;

const static double iInf = std::numeric_limits<int>::infinity();
#include <ompl/base/goals/GoalState.h>

class GoalStateFinalEdge: public ob::GoalState{
  public:
    GoalStateFinalEdge(const ob::SpaceInformationPtr &si): GoalState(si) {}

    double distanceGoal(const ob::State *qompl) const override{
      const ob::RealVectorStateSpace::StateType *qomplRnSpace = qompl->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(0);
      double d = fabs(1.0 - qomplRnSpace->values[0]);
      return d;
    }
    //void sampleGoal(State *st) const override{
    //  ob::ScopedState<> s(si_);

    //  if((this, s.get()))
    //  {
    //    const ob::RealVectorStateSpace::StateType *qomplRnSpace = s->as<ob::CompoundState>()->as<ob::RealVectorStateSpace::StateType>(0);
    //    qomplRnSpace->values[0] = 1;
    //    if (si_->satisfiesBounds(s.get()) && si_->isValid(s.get()))
    //    {
    //      //s -> state_ -> st
    //      si_->copyState(state_,s.get());
    //      si_->copyState(st, state_);
    //    }
    //  }
    //}

    //unsigned int maxSampleCount() const override{
    //  iInf;
    //}

};

namespace ompl
{
  namespace base
  {
    OMPL_CLASS_FORWARD(OptimizationObjective);
  }
  namespace geometric
  {
    class SliceSpacePRM : public base::Planner
    {
      public:
        SliceSpacePRM(RobotWorld* world_, const base::SpaceInformationPtr &si0, const ob::SpaceInformationPtr &si1);
        ~SliceSpacePRM() override;
        void setProblemDefinition(const base::ProblemDefinitionPtr &pdef) override;
        void getPlannerData(base::PlannerData &data) const override;
        base::PlannerStatus solve(const base::PlannerTerminationCondition &ptc) override;

        SliceSpace* CreateNewSliceSpaceEdge(SliceSpace::Vertex v, SliceSpace::Vertex w, double yaw);
        void clear() override;
        void setup() override;

      protected:
        SliceSpace *S_0;
        SliceSpace *S_1;

        RobotWorld *world;
        WorldPlannerSettings worldsettings;

        ob::SpaceInformationPtr si_level0;
        ob::SpaceInformationPtr si_level1;
    };
  }
}
