#pragma once
#include "elements/roadmap.h"
#include <omplapp/config.h>
#include <ompl/base/PlannerData.h>
#include <ompl/base/ProblemDefinition.h>
#include <ompl/geometric/SimpleSetup.h>
#include <ompl/geometric/PathGeometric.h>
#include <KrisLibrary/robotics/RobotKinematics3D.h> //Config

namespace ob = ompl::base;
namespace og = ompl::geometric;

struct StrategyOutput{

  public:
    StrategyOutput(CSpaceOMPL*);

    void SetShortestPath( std::vector<Config> );

    RoadmapPtr GetRoadmapPtr();
    void SetRoadmap(RoadmapPtr);

    void SetPlannerData( ob::PlannerDataPtr pd_ );
    void SetProblemDefinition( ob::ProblemDefinitionPtr pdef_ );
    ob::PlannerDataPtr GetPlannerDataPtr();
    ob::ProblemDefinitionPtr GetProblemDefinitionPtr();

    std::vector<Config> GetShortestPath();
    std::vector<std::vector<Config>> GetSolutionPaths();

    bool hasExactSolution();
    bool hasApproximateSolution();

    friend std::ostream& operator<< (std::ostream&, const StrategyOutput&);

  private:

    std::vector<Config> PathGeometricToConfigPath(og::PathGeometric &path);

    std::vector<std::vector<Config>> solution_paths;

    std::vector<Config> shortest_path;

    ob::PlannerDataPtr pd;

    ob::ProblemDefinitionPtr pdef;

    RoadmapPtr roadmap;

    CSpaceOMPL *cspace;

};