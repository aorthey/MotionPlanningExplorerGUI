import os 
import numpy as np
import sys
from math import cos,sin,pi,atan2
from urdf_create import *

limit = pi/16

headradius = 0.2
leglength1 = 0.2
leglength2 = 0.55
legradius = 0.04
jointradius = 0.1

numberLegs = 3

lowerLimit=-limit
upperLimit=limit


robot_name = 'spider/spider'
fname = getPathname(robot_name)


def createHead(headname):
  upperhead = "eye_"+headname
  tophead = "container_"+headname
  topradius = 0.6*headradius
  length = 1*headradius

  hstrs = createSphere(headname,0,0,0,headradius)
  hstrs += createRotatedCylinder(upperhead,0,0,headradius+length/2,0,0,1.57,0.3*length,length+0.1)
  hstrs += createRigidJoint( upperhead, headname)

  hstrs += createSphere(tophead,0,0,headradius+length+topradius,topradius)
  hstrs += createRigidJoint( tophead, upperhead)
  return hstrs

def createBody(headname):
  turnangle = 2*pi/numberLegs

  body = ''
  for k in range(0,numberLegs):
    angle = k*turnangle

    x = (headradius+jointradius)*cos(angle)
    y = (headradius+jointradius)*sin(angle)
    z = 0

    ez= [0,0,1]
    ey = [x,y,0]
    ex = np.cross(ez, ey)
    ex = ex/np.linalg.norm(ex)

    shouldername = headname+'_sphere_'+str(k)

    body+= createSphere(shouldername,x,y,z,jointradius)
    body+= createRigidJoint( headname, shouldername)

    leg1 = 'leg_'+str(k)+'_link0'

    x = (headradius+jointradius)*cos(angle)
    y = (headradius+jointradius)*sin(angle)
    body+= createSphericalJoint(shouldername, leg1,x,y,z,lowerLimit, upperLimit)

    x = (leglength1/2+jointradius)*cos(angle)
    y = (leglength1/2+jointradius)*sin(angle)
    body+= createRotatedCylinder(leg1,x,y,z,z,1.57,angle,legradius,leglength1-0.01) 

    x = (leglength1+2*jointradius)*cos(angle)
    y = (leglength1+2*jointradius)*sin(angle)
    body+= createSphere(leg1+'_sphere',x,y,z,jointradius)
    body+= createRigidJoint( leg1, leg1+'_sphere')

    leg2 = 'leg_'+str(k)+'_link1'

    body+= createSphericalJoint(leg1+'_sphere', leg2, x,y,z,lowerLimit, upperLimit)

    body+= createRotatedCylinder(leg2,0,0,z-leglength2/2-jointradius,0,0,angle,legradius,leglength2-0.01) 

    body+= createSphere(leg2+'_sphere',0,0,z-leglength2-2*jointradius,jointradius)
    body+= createRigidJoint( leg2, leg2+'_sphere')

  return body

################################################################################
##### write URDF robot file
################################################################################

f = open(fname,'w')

f.write('<?xml version="1.0"?>\n')
f.write('<robot name="'+robot_name+'">\n')

Njoints = 6+2+numberLegs*7

config = "config=\""+str(Njoints)+" "+" 0"*Njoints+"\""
print config

headname = "brain"
f.write(createHead(headname))
f.write(createBody(headname))
#f.write('  <klampt package_root="../.." flip_yz="1" use_vis_geom="1">\n')
f.write('  <klampt package_root="../../.." default_acc_max="4" >\n')
f.write('    <noselfcollision pairs="brain eye_brain"/>\n')
f.write('    <noselfcollision pairs="eye_brain container_brain"/>\n')
#f.write('    <link name="branch_00_spherical_joint_link" physical="0" />\n')
#f.write('    <noselfcollision pairs="eye branch_00"/>\n')
#f.write('    <noselfcollision pairs="eye branch_00_cylinder"/>\n')
f.write('  </klampt>\n')
f.write('</robot>')
f.close()
print "\nCreated new file >>",fname
