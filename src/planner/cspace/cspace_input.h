#pragma once
#include <KrisLibrary/robotics/RobotKinematics3D.h> //Config
#include "planner/planner_input.h"

struct CSpaceInput
{
  double timestep_min;
  double timestep_max;
  Config uMin;
  Config uMax;
  Config dqMin;
  Config dqMax;
  bool fixedBase{false};
  bool kinodynamic{false};
  bool multiAgent{false};
  std::vector<ContactInformation> contact_links;

  bool isTimeDependent{false};
  std::string timePathFile;
};
