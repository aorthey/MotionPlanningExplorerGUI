#pragma once
#include <ompl/base/Constraint.h>
#include "planner/cspace/cspace_geometric.h"
#include "planner/cspace/contact/ConstraintContact.h"
#include <ompl/base/spaces/constraint/ConstrainedStateSpace.h>
#include <ompl/util/RandomNumbers.h>

class GeometricCSpaceContact;

class ConstraintContactFixed : public ConstraintContact
{

public:
    ConstraintContactFixed(GeometricCSpaceContact *cspace, 
        int ambientSpaceDim, int linkNumber, 
        std::vector<Triangle3D> tris);

    void function(const Eigen::Ref<const Eigen::VectorXd> &x, Eigen::Ref<Eigen::VectorXd> out) const override;
    virtual int getNumberOfModes() override;

    // virtual int setRandomMode() override;
};
