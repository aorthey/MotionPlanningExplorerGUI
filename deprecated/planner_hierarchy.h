#pragma once

#include "planner/planner.h"
#include "elements/hierarchy.h"
#include "ViewTree.h"
#include "elements/swept_volume.h"
#include "planner/cspace_factory.h"
#include "gui_state.h"
#include "planner/planner_strategy_geometric.h"

#include <KrisLibrary/robotics/RobotKinematics3D.h> //Config
#include <KrisLibrary/utils/stringutils.h>

struct SinglePathNode{
    std::vector<Config> path;
    std::vector<ob::State*> state_path;
    bool isSufficient;

    SinglePathNode(){ sv = NULL; }
    SweptVolume& GetSweptVolume(Robot *robot){
      if(!sv){
        sv = new SweptVolume(robot, path, 0);
      }
      return *sv;
    }
    SweptVolume *sv;
};

//class ShallowPlanner: public HierarchicalMotionPlanner<SinglePathNode>
//
//class SinglePathHierarchicalPlanner: public HierarchicalMotionPlanner<SinglePathNode>
//
//class PathSpacePlanner: public HierarchicalMotionPlanner<SinglePathNode>


class HierarchicalMotionPlanner: public MotionPlanner{

  public:
    HierarchicalMotionPlanner(RobotWorld *world_, PlannerInput& input_);

    //folder-like operations on hierarchical path space
    virtual void Expand();
    virtual void Collapse();
    virtual void Next();
    virtual void Previous();
    
    virtual void DrawGL(const GUIState&);
    virtual void DrawGLScreen(double x_ =0.0, double y_=0.0);

    //planner will only be active if input exists and contains a valid algorithm
    bool isActive();
    void Print();

    //for each level: first: number of all nodes, second: node selected on that
    //level. produces a tree of nodes with distance 1 to central path
    //const std::vector<std::pair<int,int> >& GetCaterpillarTreeIndices();

  protected:
    virtual bool solve(std::vector<int> path_idxs);
    virtual std::vector< std::vector<Config> > GetSiblingPaths();
    void UpdateHierarchy();

    int current_level;
    int current_level_node;
    std::vector<int> current_path;

    Hierarchy<SinglePathNode> hierarchy;

    ViewTree viewTree;

    bool active;

};

