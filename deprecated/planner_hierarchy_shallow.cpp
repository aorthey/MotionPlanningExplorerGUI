#include "planner/planner_hierarchy_shallow.h"
#include "planner/cspace_factory.h"
#include "planner/planner_strategy_geometric.h"
#include "drawMotionPlanner.h"

ShallowMotionPlanner::ShallowMotionPlanner(RobotWorld *world_, PlannerInput& input_):
  MotionPlanner(world_,input_)
{
}
void ShallowMotionPlanner::Expand(){
  if(!active) return;

  if(current_level==0){
    WorldPlannerSettings worldsettings;
    worldsettings.InitializeDefault(*world);

    CSpaceFactory factory(input);
    CSpaceOMPL* cspace;
    SingleRobotCSpace* kcspace;

    int robot_idx = input.robot_idx;
    Robot *robot = world->robots[robot_idx];

    kcspace = new SingleRobotCSpace(*world,robot_idx,&worldsettings);

    cspace = factory.MakeGeometricCSpace(robot, kcspace);

    cspace->print();
    PlannerStrategyGeometric strategy;
    strategy.DisableOnetopicReduction();
    strategy.plan(input, cspace, output);

    output.robot = robot;

    if(output.success){
      current_level++;
      current_level_node=0;
      solution_path = output.GetKeyframes();
    }else{
      std::cout << "Error: Path could not be expanded" << std::endl;
    }
  }
}
void ShallowMotionPlanner::Collapse(){
  if(!active) return;

  if(current_level>0){
    current_level = 0;
    solution_path.clear();
  }
}

void ShallowMotionPlanner::Next(){
}
void ShallowMotionPlanner::Previous(){
}

void ShallowMotionPlanner::DrawGLScreen(double x_, double y_){
  if(!active) return;
}
void ShallowMotionPlanner::DrawGL(const GUIState& state){
  if(!active) return;

  uint ridx = input.robot_idx;
  Robot* robot = world->robots[ridx];
  const Config qi = input.q_init;
  const Config qg = input.q_goal;

  //###########################################################################
  // Draw Start/Goal Volume
  //###########################################################################
  GLColor lightGrey(0.4,0.4,0.4,0.2);
  GLColor lightGreen(0.2,0.9,0.2,0.2);
  GLColor lightRed(0.9,0.2,0.2,0.2);
  GLDraw::drawRobotAtConfig(robot, qi, lightGreen);
  GLDraw::drawRobotAtConfig(robot, qg, lightRed);

  //###########################################################################
  // Draw Current Selected Path
  //###########################################################################
  GLColor magenta(0.8,0,0.8,0.5);
  GLColor green(0.1,0.9,0.1,1);

  if(solution_path.size()>0){
    GLDraw::drawPath(solution_path, green, 20);

    const SweptVolume& sv = output.GetSweptVolume();
    GLDraw::drawGLPathSweptVolume(sv.GetRobot(), sv.GetMatrices(), sv.GetAppearanceStack(), sv.GetColor());
  }

}
