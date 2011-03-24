/*------------------------------------------------------------------------*/
/*                 Copyright 2010 Sandia Corporation.                     */
/*  Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive   */
/*  license for use of this work by or on behalf of the U.S. Government.  */
/*  Export of this program may require a license from the                 */
/*  United States Government.                                             */
/*------------------------------------------------------------------------*/

#include <stk_mesh/fixtures/GridFixture.hpp>

#include <stk_mesh/base/BulkModification.hpp>
#include <stk_mesh/base/MetaData.hpp>
#include <stk_mesh/base/BulkData.hpp>
#include <stk_mesh/base/Entity.hpp>
#include <stk_mesh/base/GetEntities.hpp>
#include <stk_mesh/base/Selector.hpp>
#include <stk_mesh/base/GetBuckets.hpp>

#include <stk_mesh/fem/TopologyHelpers.hpp>
#include <stk_mesh/fem/BoundaryAnalysis.hpp>
#include <stk_mesh/fem/SkinMesh.hpp>

#include <stk_util/parallel/ParallelReduce.hpp>

#include <use_cases/UseCase_ElementDeath_1_validation_helpers.hpp>

/*
The grid fixture creates the mesh below and skins it
1-16 Quadrilateral<4>
17-41 Nodes
skin ids are generated by the distributed index

Note:  "=" and "||" represent side entities.

17===18===19===20===21
|| 1 |  2 |  3 |  4 ||
22---23---24---25---26
|| 5 |  6 |  7 |  8 ||
27---28---29---30---31
|| 9 | 10 | 11 | 12 ||
32---33---34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

This use case will iteratively erode the mesh.

Each iteration will move a selection of faces to the 'dead_part"
Create boundaries between live and dead faces
Destroy nodes and sides that are no longer attached to a live face

0:  Init the mesh

17===18===19===20===21
|| 1 |  2 |  3 |  4 ||
22---23---24---25---26
|| 5 |  6 |  7 |  8 ||
27---28---29---30---31
|| 9 | 10 | 11 | 12 ||
32---33---34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

1: Move 4, 9 and 10 to the dead part


17===18===19===20
|| 1 |  2 |  3 ||
22---23---24---25===26
|| 5 |  6 |  7 |  8 ||
27===28===29---30---31
          || 11| 12 ||
32===33===34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41


2: Move faces 2 and 3 to the dead part

17===18
|| 1 ||
22---23===24===25===26
|| 5 |  6 |  7 |  8 ||
27===28===29---30---31
          || 11| 12 ||
32===33===34---35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

3: Move faces 1 and 11 to the dead part

22===23===24===25===26
|| 5 |  6 |  7 |  8 ||
27===28===29===30---31
               || 12||
32===33===34===35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

4: Move faces 6 and 7 to the dead part

22===23        25===26
|| 5 ||        ||  8||
27===28        30---31
               || 12||
32===33===34===35---36
|| 13| 14 | 15 | 16 ||
37===38===39===40===41

5: Move faces 5 and 16 to the dead part

               25===26
               ||  8||
               30---31
               || 12||
32===33===34===35===36
|| 13| 14 | 15 ||
37===38===39===40

6: Move the remaining faces to the dead part


(this space intentionally left blank)
  (nothing to see here)

*/

const int NUM_ITERATIONS = 7;
const int NUM_RANK = 3;

namespace {

//Finds the sides that need to be created between the live and dead entities
void find_sides_to_be_created(
    const stk::mesh::EntitySideVector & boundary,
    const stk::mesh::Selector & select,
    std::vector<stk::mesh::EntitySideComponent> & sides
    );

//Finds entities from the closure of the entities_to_be_killed
//that only have relations with dead entities
void find_lower_rank_entities_to_kill(
    const stk::mesh::EntityVector & entities_closure,
    unsigned closure_rank,
    unsigned entity_rank,
    const stk::mesh::Selector & select_owned,
    const stk::mesh::Selector & select_live,
    stk::mesh::EntityVector & kill_list
    );

}

bool element_death_use_case_1(stk::ParallelMachine pm)
{
  //set up the mesh
  stk::mesh::fixtures::GridFixture fixture(pm);

  stk::mesh::BulkData& mesh = fixture.bulk_data();
  stk::mesh::fem::FEMMetaData& fem_meta = fixture.fem_meta();
  const stk::mesh::EntityRank element_rank = fem_meta.element_rank();

  fem_meta.commit();

  mesh.modification_begin();
  fixture.generate_grid();
  mesh.modification_end();

  stk::mesh::skin_mesh(mesh, element_rank);

  // Nothing happens on iteration #0,
  // so the initial mesh should pass this validation.

  if ( ! validate_iteration( pm, fixture, 0) ) { return false ; }

  stk::mesh::Part & dead_part = *fixture.dead_part();

  stk::mesh::PartVector dead_parts;
  dead_parts.push_back( & dead_part);

  bool passed = true;

  unsigned mesh_rank = element_rank;

  for (int iteration = 0; iteration <NUM_ITERATIONS; ++iteration) {
    //find the entities to kill in this iteration
    stk::mesh::EntityVector entities_to_kill = entities_to_be_killed(mesh, iteration, element_rank);

    // find the parallel-consistent closure of the entities to be killed
    // The closure of an entity includes the entity and any lower ranked
    // entities which are reachable through relations.  For example, the
    // closure of an element consist of the element and the faces, edges,
    // and nodes that are attached to the element through relations.
    //
    // The find closure function will return a sorted parallel consistent vector
    // which contains all the entities that make up the closure of the input
    // vector.
    stk::mesh::EntityVector entities_closure;
    stk::mesh::find_closure(mesh,
        entities_to_kill,
        entities_closure);


    // find the boundary of the entities we're killing
    stk::mesh::EntitySideVector boundary;
    stk::mesh::boundary_analysis(mesh,
        entities_closure,
        mesh_rank,
        boundary);


    // Find the sides that need to be created.
    // Sides need to be created when the outside
    // of the boundary is both live and owned and
    // a side separating the live and dead doesn't
    // already exist.
    stk::mesh::Selector select_owned = fem_meta.locally_owned_part();
    stk::mesh::Selector select_live = ! dead_part ;
    stk::mesh::Selector select_live_and_owned = select_live & select_owned;

    std::vector<stk::mesh::EntitySideComponent> skin;
    find_sides_to_be_created( boundary, select_live_and_owned, skin);


    mesh.modification_begin();

    // Kill entities by moving them to the dead part.
    for (stk::mesh::EntityVector::iterator itr = entities_to_kill.begin();
        itr != entities_to_kill.end(); ++itr) {
      mesh.change_entity_parts(**itr, dead_parts);
    }


    // Ask for new entities to represent the sides between the live and dead entities
    //
    std::vector<size_t> requests(fem_meta.entity_rank_count(), 0);
    requests[mesh_rank-1] = skin.size();

    // generate_new_entities creates new blank entities of the requested ranks
    stk::mesh::EntityVector requested_entities;
    mesh.generate_new_entities(requests, requested_entities);

    // Create boundaries between live and dead entities
    // by creating a relation between the new entities and the live entities
    for ( size_t i = 0; i < skin.size(); ++i) {
      stk::mesh::Entity & entity = *(skin[i].entity);
      const unsigned side_ordinal  = skin[i].side_ordinal;
      stk::mesh::Entity & side   = * (requested_entities[i]);

      stk::mesh::declare_element_side(entity, side, side_ordinal);
    }

    mesh.modification_end();
    //the modification_end() will communicate which entities have been changed
    //to other processes.

    //find lower ranked entity that are only related to the dead entities
    //and kill them
    for (int rank = mesh_rank -1; rank >= 0; --rank) {
      stk::mesh::EntityVector kill_list;
      find_lower_rank_entities_to_kill(
          entities_closure,
          mesh_rank,
          rank,
          select_owned,
          select_live,
          kill_list
          );

      //need to communicate killing the higher ranking entities among
      //processors before killing the lower.
      mesh.modification_begin();
      for (stk::mesh::EntityVector::iterator itr = kill_list.begin();
          itr != kill_list.end(); ++itr) {
        mesh.change_entity_parts(**itr, dead_parts);
      }
      mesh.modification_end();
    }


    passed &= validate_iteration( pm, fixture, iteration);
  }

  return passed;
}

//----------------------------------------------------------------------------------
namespace {

//----------------------------------------------------------------------------------
void find_sides_to_be_created(
    const stk::mesh::EntitySideVector & boundary,
    const stk::mesh::Selector & select,
    std::vector<stk::mesh::EntitySideComponent> & sides
    )
{
  //look at the outside of the boundary since the inside will be kill this
  //iteration

  for (stk::mesh::EntitySideVector::const_iterator itr = boundary.begin();
      itr != boundary.end(); ++itr) {

    const stk::mesh::EntitySideComponent & outside = itr->outside;


    // examine the boundary of the outside of the closure.
    if ( outside.entity != NULL && select(*(outside.entity)) ) {

      //make sure the side does not already exist
      const unsigned side_ordinal = outside.side_ordinal;
      const stk::mesh::Entity & entity = * outside.entity;
      stk::mesh::PairIterRelation existing_sides = entity.relations(entity.entity_rank()-1);

      for (; existing_sides.first != existing_sides.second &&
          existing_sides.first->identifier() != side_ordinal ;
          ++existing_sides.first);

      //reached the end -- a new side needs to be created
      if (existing_sides.first == existing_sides.second) {
        sides.push_back(outside);
      }
    }
  }
}

//----------------------------------------------------------------------------------
void find_lower_rank_entities_to_kill(
    const stk::mesh::EntityVector & entities_closure,
    unsigned mesh_rank,
    unsigned entity_rank,
    const stk::mesh::Selector & select_owned,
    const stk::mesh::Selector & select_live,
    stk::mesh::EntityVector & kill_list
    )
{

  kill_list.clear();

  //find the first entity in the closure
  stk::mesh::EntityVector::const_iterator itr = std::lower_bound(entities_closure.begin(),
      entities_closure.end(),
      stk::mesh::EntityKey(entity_rank, 0),
      stk::mesh::EntityLess());

  const stk::mesh::EntityVector::const_iterator end = std::lower_bound(entities_closure.begin(),
      entities_closure.end(),
      stk::mesh::EntityKey(entity_rank+1, 0),
      stk::mesh::EntityLess());

  for (; itr != end; ++itr) {
    stk::mesh::Entity & entity = **itr;

    if (select_owned(entity.bucket())) {
      bool found_live = false;

      for(unsigned rank = entity_rank + 1; rank<=mesh_rank && !found_live; ++rank) {

        stk::mesh::PairIterRelation relations_pair = entity.relations(rank);

        for (; relations_pair.first != relations_pair.second && !found_live; ++relations_pair.first) {

          if( select_live(*relations_pair.first->entity())) {
            found_live = true;
          }
        }
      }

      if (!found_live) {
        kill_list.push_back(&entity);
      }
    }
  }

}

}
