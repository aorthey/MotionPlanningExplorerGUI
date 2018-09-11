#include "common.h"
#include "qng.h"
#include "elements/plannerdata_vertex_annotated.h"
#include "planner/cspace/validitychecker/validity_checker_ompl.h"
#include <limits>
#include <boost/graph/astar_search.hpp>
#include <boost/graph/incremental_components.hpp>
#include <boost/property_map/vector_property_map.hpp>
#include <boost/property_map/transform_value_property_map.hpp>
#include <boost/foreach.hpp>

#define foreach BOOST_FOREACH

using namespace ompl::geometric;

QNG::QNG(const base::SpaceInformationPtr &si, Quotient *parent ): BaseT(si, parent)
{
  setName("QNG"+std::to_string(id));
  if (!nearest_cover){
    nearest_cover.reset(tools::SelfConfig::getDefaultNearestNeighbors<Configuration *>(this));
    nearest_cover->setDistanceFunction([this](const Configuration *a, const Configuration *b)
                             {
                                return DistanceNeighborhoodNeighborhood(a,b);
                              });
  }
  if (!nearest_vertex){
    nearest_vertex.reset(tools::SelfConfig::getDefaultNearestNeighbors<Configuration *>(this));
    nearest_vertex->setDistanceFunction([this](const Configuration *a, const Configuration *b)
                             {
                                return DistanceConfigurationConfiguration(a,b);
                              });
  }
}

QNG::~QNG(void)
{
}
//#############################################################################
//SETUP
//#############################################################################

void QNG::setup(void)
{

  if (pdef_){
    //#########################################################################
    if (pdef_->hasOptimizationObjective()){
      opt_ = pdef_->getOptimizationObjective();
    }else{
      OMPL_ERROR("%s: Did not specify optimization function.", getName().c_str());
      exit(0);
    }
    //#########################################################################
    //Parent data if available
    //#########################################################################
    Configuration *coset_start = nullptr;
    Configuration *coset_goal = nullptr;
    if(parent != nullptr)
    {
      coset_start = static_cast<og::QNG*>(parent)->q_start;
      coset_goal = static_cast<og::QNG*>(parent)->q_goal;
    }
    //#########################################################################
    //Adding start configuration
    //#########################################################################
    if(const ob::State *state_start = pis_.nextStart()){
      q_start = CreateConfigurationFromStateAndCoset(state_start, coset_start);
      if(q_start == nullptr){
        OMPL_ERROR("%s: Could not add start state!", getName().c_str());
        exit(0);
      }
      v_start = AddConfigurationToCover(q_start);
    }else{
      OMPL_ERROR("%s: There are no valid initial states!", getName().c_str());
      exit(0);
    }

    //#########################################################################
    //Adding goal configuration
    //#########################################################################
    if(const ob::State *state_goal = pis_.nextGoal()){
      q_goal = CreateConfigurationFromStateAndCoset(state_goal, coset_goal);
      if(q_goal == nullptr){
        OMPL_ERROR("%s: Could not add goal state!", getName().c_str());
        exit(0);
      }
    }else{
      OMPL_ERROR("%s: There are no valid goal states!", getName().c_str());
      exit(0);
    }

    q_start->isStart = true;
    q_goal->isGoal = true;

    //#########################################################################
    //Check saturation
    //#########################################################################
    if(q_start->GetRadius() == std::numeric_limits<double>::infinity())
    {
      OMPL_INFORM("Note: start state covers quotient-space.");
      saturated = true;
    }

    //#########################################################################
    OMPL_INFORM("%s: ready with %lu states already in datastructure", getName().c_str(), nearest_cover->size());
    setup_ = true;
    std::cout << "start: " << v_start << " goal: " << v_goal << std::endl;
  }else{
    setup_ = false;
  }

}

//#############################################################################
//Configuration methods
//#############################################################################
QNG::Vertex QNG::AddConfigurationToCover(Configuration *q)
{
  //get all neighbors before adding q (otherwise q might be neighbor of itself)
  std::vector<Configuration*> neighbors = GetConfigurationsInsideNeighborhood(q);

  //(1) add to cover graph
  Vertex v = boost::add_vertex(q, graph);
  graph[v]->number_attempted_expansions = 0;
  graph[v]->number_successful_expansions = 0;

  //assign vertex to a unique index
  put(vertexToIndex, v, index_ctr);
  put(indexToVertex, index_ctr, v);
  graph[v]->index = index_ctr;
  index_ctr++;

  //(2) add to nearest neighbor structure
  nearest_cover->add(q);
  nearest_vertex->add(q);

  //(3) add to PDF
  PDF_Element *q_element = pdf_all_configurations.add(q, q->GetImportance());
  q->SetPDFElement(q_element);
  if(!q->isSufficientFeasible){
    PDF_Element *q_necessary_element = pdf_necessary_configurations.add(q, q->GetRadius());
    q->SetNecessaryPDFElement(q_necessary_element);
  }

  std::cout << std::string(80, '-') << std::endl;
  std::cout << "*** ADD VERTEX " << q->index << std::endl;

  //Clean UP and Connect
  //(1) Remove All Configurations with neighborhoods inside current neighborhood
  //(2) Add Edges to All Configurations with centers inside the current neighborhood 
  std::cout << "Vertex: " << q->index << " has number of neighbors: " << neighbors.size() << std::endl;
  std::cout << std::string(80, '-') << std::endl;
  for(uint k = 0; k < neighbors.size(); k++){
    Configuration *qn = neighbors.at(k);
    if(qn==q){
      std::cout << "configuration equals neighbor." << std::endl;
      exit(0);
    }
    std::cout << "Vertex" << q->index << " and neighbor " << qn->index << std::endl;
    if(IsNeighborhoodInsideNeighborhood(qn, q))
    {
      //do not delete start/goal
      if(qn->isStart){
        AddEdge(q, qn);
      }else if(qn->isGoal){
        AddEdge(q, qn);
      }else{
        //old constellation
        //v1 -------                                    |
        //          \                                   |
        //v2-------- vn ------- vq                      |
        //          /                                   |
        //v3--------                                    |
        //
        //new constellation
        //v1 ---------------                            |
        //                  \                           |
        //v2--------------- vq                          |
        //                  /                           |
        //v3----------------                            |
        //
        //get all edges into qn, and rewire them to q
        Vertex vq = get(indexToVertex, q->index);
        Vertex vn = get(indexToVertex, qn->index);
        // Vertex vq = indexToVertex(q->index);
        // Vertex vn = indexToVertex(qn->index);
        OEIterator edge_iter, edge_iter_end, next; 
        boost::tie(edge_iter, edge_iter_end) = boost::out_edges(vn, graph);

        std::cout << "removing " << boost::out_degree(vn, graph) << " edges from " << qn->index << std::endl;

        for(next = edge_iter; edge_iter != edge_iter_end; edge_iter = next)
        {
          //add v_target to vq
          ++next;
          const Vertex v_target = boost::target(*edge_iter, graph);
          double d = DistanceQ1(graph[v_target], q);
          EdgeInternalState properties(d);
          std::cout << "add " << get(vertexToIndex, vq) << "-" << get(vertexToIndex, v_target) << std::endl;
          boost::add_edge(v_target, vq, properties, graph);
        }
        RemoveConfigurationFromCover(qn);
      }
    }else{
      AddEdge(q, qn);
    }
  }
  std::cout << "adding parent neighbor if any" << std::endl;
  if(q->parent_neighbor != nullptr){
    //check that vertex has not been previously deleted
    if(indexToVertexStdMap.count(q->parent_neighbor->index) > 0){
      AddEdge(q, q->parent_neighbor);
    }
  }
  return v;
}
void QNG::AddEdge(Configuration *q_from, Configuration *q_to)
{
  std::cout << "adding edge " << q_from->index << " - " << q_to->index << std::endl;
  double d = DistanceQ1(q_from, q_to);
  EdgeInternalState properties(d);
  Vertex v_from = get(indexToVertex, q_from->index);
  Vertex v_to = get(indexToVertex, q_to->index);
  boost::add_edge(v_from, v_to, properties, graph);
}

void QNG::RemoveConfigurationFromCover(Configuration *q)
{
  //(1) Remove from PDF
  pdf_all_configurations.remove(static_cast<PDF_Element*>(q->GetPDFElement()));
  if(!q->isSufficientFeasible){
    pdf_necessary_configurations.remove(static_cast<PDF_Element*>(q->GetNecessaryPDFElement()));
  }
  //(2) Remove from nearest neighbors structure
  nearest_cover->remove(q);
  nearest_vertex->remove(q);

  //(3) Remove from cover graph
  q->Remove(Q1);

  std::cout << "**** removing " << q->index << std::endl;

  //erase entry from indexmap
  Vertex vq = get(indexToVertex, q->index);
  vertexToIndexStdMap.erase(vq);
  indexToVertexStdMap.erase(q->index);

  //check that they do not exist in indextovertex anymore
  if(indexToVertexStdMap.count(q->index) > 0){
    vertex_index_type vit_before = get(vertexToIndex, vq);
    vertex_index_type vit_after = get(vertexToIndex, vq);
    std::cout << vit_before << " - " << vit_after << std::endl;
    exit(0);
  }

  boost::remove_vertex(vq, graph);

  delete q;
}


QNG::Configuration* QNG::CreateConfigurationFromStateAndCoset(const ob::State *state, Configuration *q_coset)
{
  Configuration *q = new Configuration(Q1, state);

  //cannot create configurations inside cover
  if(IsConfigurationInsideCover(q)){
    q->Remove(Q1);
    return nullptr;
  }
  q->coset = q_coset;

  bool feasible = Q1->isValid(q->state);

  if(feasible){
    if(IsOuterRobotFeasible(q->state))
    {
      q->isSufficientFeasible = true;
      q->SetRadius(DistanceOuterRobotToObstacle(q->state));
    }else{
      q->SetRadius(DistanceInnerRobotToObstacle(q->state));
    }
    if(q->GetRadius()<threshold_clearance){
      q->Remove(Q1);
      return nullptr;
    }
  }else{
    q->Remove(Q1);
    return nullptr;
  }

  return q;
}
void QNG::RemoveConfigurationsFromCoverCoveredBy(Configuration *q)
{
  //If the neighborhood of q is a superset of any other neighborhood, then delete
  //the other neighborhood, and rewire the graph
  std::vector<Configuration*> neighbors = GetConfigurationsInsideNeighborhood(q);

  for(uint k = 0; k < neighbors.size(); k++){

    Configuration *qn = neighbors.at(k);

    //do not delete start/goal
    if(qn->isStart) continue;
    if(qn->isGoal) continue;

    if(IsNeighborhoodInsideNeighborhood(qn, q))
    {
      RemoveConfigurationFromCover(qn);
    }
  }
}


bool QNG::IsConfigurationInsideNeighborhood(Configuration *q, Configuration *qn)
{
  return (DistanceConfigurationConfiguration(q, qn) <= 0);
}

bool QNG::IsConfigurationInsideCover(Configuration *q)
{
  std::vector<Configuration*> neighbors;

  nearest_cover->nearestK(q, 2, neighbors);

  if(neighbors.size()<=1) return false;

  return IsConfigurationInsideNeighborhood(q, neighbors.at(0)) && IsConfigurationInsideNeighborhood(q, neighbors.at(1));
}

void QNG::Grow(double t)
{
  ob::PlannerTerminationCondition ptc( ob::timedPlannerTerminationCondition(t) );
  //#########################################################################
  //Do not grow the cover if it is saturated, i.e. it cannot be expanded anymore
  // --- we have successfully computed Q1_{free}, the free space of Q1
  //#########################################################################
  if(saturated) return;

  //#########################################################################
  //Sample a configuration different from the current cover
  //#########################################################################

  Configuration *q_random = nullptr;
  while(q_random == nullptr && !ptc){
    q_random = Sample();
  }

  if(q_random == nullptr) return;

  //#########################################################################
  //Nearest in cover
  //#########################################################################
  Configuration *q_nearest = Nearest(q_random);

  if(q_nearest->state == q_random->state){
    OMPL_ERROR("sampled and nearest states are equivalent.");
    std::cout << "sampled state" << std::endl;
    Q1->printState(q_random->state);
    std::cout << "nearest state" << std::endl;
    Q1->printState(q_nearest->state);
    exit(0);
  }

  // //#########################################################################
  // //Interpolate nearest to random --- while staying in neighborhood of q_nearest
  // //#########################################################################
  Interpolate(q_nearest, q_random);

  // //#########################################################################
  // //Try to expand further in that direction until we hit an infeasible point, or
  // //the time is up
  // //
  // //  q_nearest  ---->  q_random  ---->  q_next
  // //#########################################################################

  AddConfigurationToCover(q_random);
  //nearest could have been removed!

  //
  // if(AddConfiguration(q_random, q_nearest)){
  //   const uint max_extension_steps = 1000;
  //   uint step = 0;
  //   while(step++ < max_extension_steps)
  //   {
  //     q_random->number_attempted_expansions++;
  //     pdf_all_configurations.update(static_cast<PDF_Element*>(q_random->GetPDFElement()), q_random->GetImportance());

  //     q_nearest = q_random->parent;
  //     Configuration *q_next = EstimateBestNextState(q_nearest, q_random);

  //     if(q_next == nullptr)
  //     {
  //       if(verbose>1) std::cout << "q_next is nullptr" << std::endl;
  //       break;
  //     }
  //     //note that q_random might have been removed if the neighborhood is a
  //     //proper subset of the neighborhood of q_next
  //     if(q_random == nullptr){
  //       Connect(q_nearest, q_next);
  //     }else{
  //       Connect(q_random, q_next);
  //       pdf_all_configurations.update(static_cast<PDF_Element*>(q_random->GetPDFElement()), q_random->GetImportance());
  //     }
  //     q_random = q_next;
  //   }
  // }

  // //update PDF
  // if(q_nearest != nullptr){
  //   pdf_all_configurations.update(static_cast<PDF_Element*>(q_nearest->GetPDFElement()), q_nearest->GetImportance());
  // }
}

QNG::Configuration* QNG::EstimateBestNextState(Configuration *q_last, Configuration *q_current)
{
  std::cout << "NYI" << std::endl;
  exit(0);
    // uint K_samples = 3; //how many samples to test for best direction (depends maybe also on radius)

    // Configuration *q_next = new Configuration(Q1);

    // if(verbose>1){
    //   std::cout << "from" << std::endl;
    //   Q1->printState(q_last->state);
    //   std::cout << "to" << std::endl;
    //   Q1->printState(q_current->state);
    // }

    // double d_last_to_current = Distance(q_last, q_current);
    // double radius_current = q_current->GetRadius();
    // Q1->getStateSpace()->interpolate(q_last->state, q_current->state, 1 + radius_current/d_last_to_current, q_next->state);

    // //#######################################################################
    // // We sample the distance function to obtain K samples {q_1,\cdots,q_K}.
    // // For each $q_k$ we compute the value of the distance function dk_radius.
    // // Then we employ two strategies to choose the next sample to follow:
    // //
    // //either follow isolines : min( fabs(radius_k_sample - current_radius) )
    // //or follow the steepest ascent: max(radius_k_sample)

    // double radius_best = DistanceInnerRobotToObstacle(q_next->state);
    // if(verbose>1){
    //   std::cout << "next" << std::endl;
    //   Q1->printState(q_next->state);
    //   std::cout << "radius " << radius_best << std::endl;
    // }

    // //double radius_best = q_next->GetRadius();
    // double SAMPLING_RADIUS = 0.1*radius_current;
    // double radius_ratio = radius_best / radius_current;
    // if(radius_ratio > 1){
    //   //we encountered a bigger radius neighborhood. this is great, we should go
    //   //into that direction
    //   if(!AddConfiguration(q_next))
    //   {
    //     return nullptr;
    //   }
    //   return q_next;
    // }else{
    //   //if next neighborhood is exceptionally small, try to sample more broadly.
    //   //Otherwise sample smaller. This corresponds to having more speed in the
    //   //momentum of sampling.
    //   if(radius_ratio > 0.1){
    //     SAMPLING_RADIUS = (1.0-radius_ratio+0.1)*radius_current;
    //   }else{
    //     SAMPLING_RADIUS = radius_current;
    //   }
    // }

    // //#######################################################################
    // //keep q_next constant
    // q_next = const_cast<Configuration*>(q_next);
    // Configuration *q_best = q_next;

    // for(uint k = 0; k < K_samples; k++)
    // {
    //   //obtain sample q_k, and radius radius_k
    //   Configuration *q_k = new Configuration(Q1);
    //   Q1_sampler->sampleUniformNear(q_k->state, q_next->state, SAMPLING_RADIUS);
    //   double d_current_to_k = DistanceQ1(q_k, q_current);
    //   Q1->getStateSpace()->interpolate(q_current->state, q_k->state, radius_current/d_current_to_k, q_k->state);

    //   double radius_next = DistanceInnerRobotToObstacle(q_k->state);

    //   if(verbose>1){
    //     std::cout << "q_k" << std::endl;
    //     Q1->printState(q_k->state);
    //     std::cout << "radius: " << radius_next << std::endl;
    //   }

    //   if(q_best->isSufficientFeasible && !q_k->isSufficientFeasible)
    //   {
    //     q_best = q_k;
    //     radius_best = radius_next;
    //   }else{
    //     if(radius_next > radius_best)
    //     {
    //       q_best = q_k;
    //       radius_best = radius_next;
    //     }else{
    //       q_k->Remove(Q1);
    //       continue;
    //     }
    //   }
    // }
    // if(!AddConfiguration(q_best))
    // {
    //   return nullptr;
    // }
    // //#########################################################################
    // //return q_next
    // //#########################################################################
    // return q_best;
}

//#############################################################################
//Sampling Methods
//#############################################################################
QNG::Configuration* QNG::Sample(){
  ob::State *state = Q1->allocState();
  Configuration *q_coset = nullptr;
  Configuration *q_parent = nullptr;

  if(parent == nullptr){

    if(!hasSolution){
      //Stage1 Sampling: Three strategies
      //  (1) Goal Bias
      //  (2) Voronoi Bias
      //  (3) Expansion Bias

      double r = rng_.uniform01();
      if(r<goalBias){
        state = si_->cloneState(q_goal->state);
      }else{
        if(r < goalBias + (1-goalBias)*voronoiBias){
          Q1_sampler->sampleUniform(state);
        }else{
          //choose a random configuration from the cover
          Configuration *q;
          if(pdf_necessary_configurations.size()>0){
            q = pdf_necessary_configurations.sample(rng_.uniform01());
            q->number_attempted_expansions++;
            pdf_necessary_configurations.update(static_cast<PDF_Element*>(q->GetNecessaryPDFElement()), q->GetImportance());
          }else{
            q = pdf_all_configurations.sample(rng_.uniform01());
            q->number_attempted_expansions++;
            pdf_all_configurations.update(static_cast<PDF_Element*>(q->GetPDFElement()), q->GetImportance());
          }
          //sample its boundary
          sampleHalfBallOnNeighborhoodBoundary(state, q);
          q_parent = q;
        }
      }
    }else{
      //State2 Sampling
      std::cout << "NYI" << std::endl;
      exit(0);
    }

  }else{
    //Q1 = X0 x X1
    //Q0 = X0
    ob::State *stateX1 = X1->allocState();
    ob::State *stateQ0 = Q0->allocState();
    X1_sampler->sampleUniform(stateX1);
    q_coset = static_cast<og::QNG*>(parent)->SampleQuotientCover(stateQ0);
    if(q_coset->coset == nullptr){
      OMPL_ERROR("no coset found for state");
      Q1->printState(state);
      exit(0);
    }
    MergeStates(stateQ0, stateX1, state);
    X1->freeState(stateX1);
    Q0->freeState(stateQ0);
    //return true;
  }
  Configuration* q_random = CreateConfigurationFromStateAndCoset(state, q_coset);
  Q1->freeState(state);
  if(q_random!=nullptr) q_random->parent_neighbor = q_parent;
  return q_random;
}

//return state in Q0 and the nearest configuration
QNG::Configuration* QNG::SampleQuotientCover(ob::State *state) const
{
  std::cout << "sample quotient cover" << std::endl;
  std::cout << "NYI" << std::endl;
  exit(0);
}

bool QNG::sampleHalfBallOnNeighborhoodBoundary(ob::State *state, const Configuration *center)
{
  //sample on boundary of open neighborhood
  // (1) first sample gaussian point qk around q
  // (2) project qk onto neighborhood
  // (*) this works because gaussian is symmetric around origin
  // (*) this does not work when qk is near to q, so we need to sample as long
  // as it is not near
  //
  double radius = center->GetRadius();

  double dist_q_qk = 0;

  //@DEBUG
  if(epsilon_min_distance >= radius){
    std::cout << std::string(80, '-') << std::endl;
    OMPL_ERROR("neighborhood is too small to sample the boundary.");

    std::cout << std::string(80, '-') << std::endl;
    std::cout << "states currently considered:" << std::endl;
    for(uint k = 0; k < pdf_all_configurations.size(); k++){
      Configuration *q = pdf_all_configurations[k];
      std::cout << "state " << k << " with radius " << q->GetRadius() << " has values:" << std::endl;
      Q1->printState(q->state);
    }
    std::cout << std::string(80, '-') << std::endl;
    
    std::cout << "neighborhood radius: " << radius << std::endl;
    std::cout << "minimal distance to be able to sample: " << epsilon_min_distance << std::endl;
    exit(0);
  }

  //sample as long as we are inside the ball of radius epsilon_min_distance
  while(dist_q_qk <= epsilon_min_distance){
    Q1_sampler->sampleGaussian(state, center->state, 1);
    dist_q_qk = si_->distance(center->state, state);
  }

  //S = sample->state
  //C = center->state
  //P = center->parent->state
  si_->getStateSpace()->interpolate(center->state, state, radius/dist_q_qk, state);
  return true;

}
bool QNG::sampleUniformOnNeighborhoodBoundary(ob::State *state, const Configuration *center)
{
  //sample on boundary of open neighborhood
  // (1) first sample gaussian point qk around q
  // (2) project qk onto neighborhood
  // (*) this works because gaussian is symmetric around origin
  // (*) this does not work when qk is near to q, so we need to sample as long
  // as it is not near
  //
  double radius = center->openNeighborhoodRadius;

  double dist_q_qk = 0;

  //@DEBUG
  if(epsilon_min_distance >= radius){
    OMPL_ERROR("neighborhood is too small to sample the boundary.");
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "states currently considered:" << std::endl;
    for(uint k = 0; k < pdf_all_configurations.size(); k++){
      Configuration *q = pdf_all_configurations[k];
      std::cout << "state " << k << " with radius " << q->GetRadius() << " has values:" << std::endl;
      Q1->printState(q->state);
    }
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "neighborhood radius: " << radius << std::endl;
    std::cout << "minimal distance to be able to sample: " << epsilon_min_distance << std::endl;
    std::cout << "state:" << std::endl;
    Q1->printState(center->state);
    exit(0);
  }

  //sample as long as we are inside the ball of radius epsilon_min_distance
  while(dist_q_qk <= epsilon_min_distance){
    Q1_sampler->sampleGaussian(state, center->state, 1);
    dist_q_qk = si_->distance(center->state, state);
  }
  si_->getStateSpace()->interpolate(center->state, state, radius/dist_q_qk, state);

  return true;
}


//#############################################################################
//Distance
//#############################################################################

bool QNG::IsNeighborhoodInsideNeighborhood(Configuration *lhs, Configuration *rhs)
{
  double distance_centers = DistanceQ1(lhs, rhs);
  double radius_rhs = rhs->GetRadius();
  double radius_lhs = lhs->GetRadius();
  return (radius_rhs > (radius_lhs + distance_centers));
}

std::vector<QNG::Configuration*> QNG::GetConfigurationsInsideNeighborhood(Configuration *q)
{
  std::vector<Configuration*> neighbors;
  nearest_vertex->nearestR(q, q->GetRadius()+threshold_clearance*0.5, neighbors);
  return neighbors;
}


QNG::Configuration* QNG::Nearest(Configuration *q) const
{
  return nearest_cover->nearest(q);
}

//@TODO: should be fixed to reflect the distance on the underlying
//quotient-space, i.e. similar to QMPConnect. For now we use DistanceQ1
bool QNG::Interpolate(const Configuration *q_from, Configuration *q_to)
{
  double d = DistanceQ1(q_from, q_to);
  double radius = q_from->GetRadius();
  double step_size = radius/d;
  si_->getStateSpace()->interpolate(q_from->state, q_to->state, step_size, q_to->state);
  return true;
}

double QNG::DistanceConfigurationConfiguration(const Configuration *q_from, const Configuration *q_to)
{
  if(parent == nullptr){
    //the very first quotient-space => usual metric
    return DistanceQ1(q_from, q_to);
  }else{
    if(q_to->coset == nullptr || q_from->coset == nullptr)
    {
      //could not find a coset on the quotient-space  => usual metric
      return DistanceQ1(q_from, q_to);
    }
    //metric defined by distance on cover of quotient-space plus distance on the
    //subspace Q1/Q0
    og::QNG *QNG_parent = dynamic_cast<og::QNG*>(parent);
    return QNG_parent->DistanceOverCover(q_from->coset, q_to->coset)+DistanceX1(q_from, q_to);
  }
}

double QNG::DistanceQ1(const Configuration *q_from, const Configuration *q_to)
{
  return Q1->distance(q_from->state, q_to->state);
}

double QNG::DistanceX1(const Configuration *q_from, const Configuration *q_to)
{
  ob::State *stateFrom = X1->allocState();
  ob::State *stateTo = X1->allocState();
  ExtractX1Subspace(q_from->state, stateFrom);
  ExtractX1Subspace(q_to->state, stateTo);
  double d = X1->distance(stateFrom, stateTo);
  X1->freeState(stateFrom);
  X1->freeState(stateTo);
  return d;
}

double QNG::DistanceConfigurationNeighborhood(const Configuration *q_from, const Configuration *q_to)
{
  double d_to = q_to->GetRadius();
  double d = DistanceConfigurationConfiguration(q_from, q_to);
  return std::max(d - d_to, 0.0);
}
double QNG::DistanceNeighborhoodNeighborhood(const Configuration *q_from, const Configuration *q_to)
{
  double d_from = q_from->GetRadius();
  double d_to = q_to->GetRadius();
  double d = si_->distance(q_from->state, q_to->state);

  if(d!=d){
    std::cout << std::string(80, '-') << std::endl;
    std::cout << "NaN detected." << std::endl;
    std::cout << "d_from " << d_from << std::endl;
    std::cout << "d_to " << d_to << std::endl;
    std::cout << "d " << d << std::endl;
    std::cout << "configuration 1: " << std::endl;
    Q1->printState(q_from->state);
    std::cout << "configuration 2: " << std::endl;
    Q1->printState(q_to->state);
    std::cout << std::string(80, '-') << std::endl;

    exit(1);
  }
  double d_open_neighborhood_distance = std::max(d - d_from - d_to, 0.0); 
  return d_open_neighborhood_distance;
}

double QNG::DistanceOverCover(const Configuration *q_from, const Configuration *q_to)
{
  std::cout << "distance quotient cover" << std::endl;
  std::cout << "NYI" << std::endl;
  exit(0);

}


void QNG::Init()
{
  checkValidity();
}


void QNG::clear()
{
  Planner::clear();
  if(nearest_cover){
    nearest_cover->clear();
  }
  if(nearest_vertex){
    nearest_vertex->clear();
  }
  hasSolution = false;
  q_start = nullptr;
  q_goal = nullptr;

  pis_.restart();
}

bool QNG::GetSolution(ob::PathPtr &solution)
{
  //create solution only if we already found a path
  if(hasSolution){
    std::vector<Vertex> vpath = GetCoverPath(v_start, v_goal);
    og::PathGeometric gpath = static_cast<og::PathGeometric&>(*solution);
    gpath.clear();
    for(uint k = 0; k < vpath.size(); k++){
      gpath.append(graph[vpath.at(k)]->state);
    }
  }
  return hasSolution;
}

void QNG::CopyChartFromSibling( QuotientChart *sibling, uint k )
{
  std::cout << "copy chart " << std::endl;
  std::cout << "NYI" << std::endl;
  exit(0);
}

QNG::Configuration* QNG::GetStartConfiguration() const
{
  return q_start;
}
QNG::Configuration* QNG::GetGoalConfiguration() const
{
  return q_goal;
}

const QNG::PDF& QNG::GetPDFNecessaryVertices() const
{
  return pdf_necessary_configurations;
}

const QNG::PDF& QNG::GetPDFAllVertices() const
{
  return pdf_all_configurations;
}

double QNG::GetGoalBias() const
{
  return goalBias;
}

struct found_goal {}; // exception for termination

template <class Vertex>
class astar_goal_visitor : public boost::default_astar_visitor
{
public:
  astar_goal_visitor(Vertex goal) : m_goal(goal) {}
  template <class Graph>
  void examine_vertex(Vertex u, Graph& g) {
    if(u == m_goal)
      throw found_goal();
  }
private:
  Vertex m_goal;
};


std::vector<QNG::Vertex> QNG::GetCoverPath(const Vertex& start, const Vertex& goal)
{
  std::vector<Vertex> path;

  std::cout << "finding path between " << start << " - " << goal << std::endl;
  Q1->printState(graph[start]->state);
  Q1->printState(graph[goal]->state);

    //vector<Vertex> p(num_vertices(G), graph_traits<G>::null_vertex()); //the predecessor array
  std::vector<Vertex> prev(boost::num_vertices(graph), boost::graph_traits<Graph>::null_vertex());
  auto weight = boost::make_transform_value_property_map(std::mem_fn(&EdgeInternalState::getCost), get(boost::edge_bundle, graph));
  auto predecessor = boost::make_iterator_property_map(prev.begin(), vertexToIndex);

  try{
  boost::astar_search(graph, start,
                    [this, goal](const Vertex &v)
                    {
                        return ob::Cost(DistanceQ1(graph[v], graph[goal]));
                    },
                    //boost::predecessor_map(&prev[0])
                      predecessor_map(predecessor)
                      .weight_map(weight)
                      .visitor(astar_goal_visitor<Vertex>(goal))
                      .vertex_index_map(vertexToIndex)
                      .distance_compare([this](EdgeInternalState c1, EdgeInternalState c2)
                                        {
                                            return opt_->isCostBetterThan(c1.getCost(), c2.getCost());
                                        })
                      .distance_combine([this](EdgeInternalState c1, EdgeInternalState c2)
                                        {
                                            return opt_->combineCosts(c1.getCost(), c2.getCost());
                                        })
                      .distance_inf(opt_->infiniteCost())
                      .distance_zero(opt_->identityCost())
                    );
  }catch(found_goal fg){
    //std::cout << prev[goal] << std::endl;
    // for(Vertex v = goal;; v = prev[v])
    // {
    //   path.push_back(v);
    //   if(prev[v] == v)
    //     break;
    // }
    std::cout << "found goal" << std::endl;
    //list<Vertex>::iterator spi = shortest_path.begin();
  }

  // if (prev[goal.index] == goal){
  //   return path;
  // }
  // for(Vertex pos = goal; prev[pos] != pos; pos = prev[pos]){
  //   path.push_back(pos);
  // }
  // path.push_back(start);
  // std::reverse(path.begin(), path.end());
  return path;
}


PlannerDataVertexAnnotated QNG::getAnnotatedVertex(ob::State* state, double radius, bool sufficient) const
{
  PlannerDataVertexAnnotated pvertex(state);
  pvertex.SetLevel(GetLevel());
  pvertex.SetPath(GetChartPath());
  pvertex.SetOpenNeighborhoodDistance(radius);

  if(!state){
    std::cout << "vertex state does not exists" << std::endl;
    Q1->printState(state);
    exit(0);
  }

  using FeasibilityType = PlannerDataVertexAnnotated::FeasibilityType;
  if(sufficient){
    pvertex.SetFeasibility(FeasibilityType::SUFFICIENT_FEASIBLE);
  }else{
    pvertex.SetFeasibility(FeasibilityType::FEASIBLE);
  }
  return pvertex;
}
PlannerDataVertexAnnotated QNG::getAnnotatedVertex(Vertex vertex) const
{
  ob::State *state = (isLocalChart?si_->cloneState(graph[vertex]->state):graph[vertex]->state);
  return getAnnotatedVertex(state, graph[vertex]->GetRadius(), graph[vertex]->isSufficientFeasible);
}


void QNG::getPlannerDataAnnotated(base::PlannerData &data) const
{
  std::cout << "graph has " << boost::num_vertices(graph) << " vertices and " << boost::num_edges(graph) << " edges." << std::endl;
  std::map<const Vertex, const ob::State*> vertexToStates;

  PlannerDataVertexAnnotated pstart = getAnnotatedVertex(v_start);
  vertexToStates[v_start] = pstart.getState();
  data.addStartVertex(pstart);

  // if(hasSolution){
  //   PlannerDataVertexAnnotated pgoal = getAnnotatedVertex(v_goal);
  //   data.addGoalVertex(pgoal);
  //   std::cout << "hasSolution " << data.vertexIndex(pgoal) << std::endl;
  // }else{
  //   PlannerDataVertexAnnotated pgoal = getAnnotatedVertex(q_goal->state, q_goal->GetRadius(), q_goal->isSufficientFeasible);
  //   data.addGoalVertex(pgoal);
  // }

  foreach( const Vertex v, boost::vertices(graph))
  {
    if(vertexToStates.find(v) == vertexToStates.end()) {
      PlannerDataVertexAnnotated p = getAnnotatedVertex(v);
      data.addVertex(p);
      vertexToStates[v] = p.getState();
    }
    //otherwise vertex is a goal or start vertex and has already been added
  }
  foreach (const Edge e, boost::edges(graph))
  {
    const Vertex v1 = boost::source(e, graph);
    const Vertex v2 = boost::target(e, graph);

    const ob::State *s1 = vertexToStates[v1];
    const ob::State *s2 = vertexToStates[v2];
    PlannerDataVertexAnnotated p1(s1);
    PlannerDataVertexAnnotated p2(s2);
    data.addEdge(p1,p2);
  }
  std::cout << "added " << data.numVertices() << " vertices and " << data.numEdges() << " edges."<< std::endl;
}
