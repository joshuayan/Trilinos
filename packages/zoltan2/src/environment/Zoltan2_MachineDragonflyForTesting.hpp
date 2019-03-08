#ifndef _ZOLTAN2_MACHINE_DRAGONFLYTEST_HPP_
#define _ZOLTAN2_MACHINE_DRAGONFLYTEST_HPP_

#include <Teuchos_Comm.hpp>
#include <Teuchos_CommHelpers.hpp>
#include <Zoltan2_Machine.hpp>

#include <cstdlib>    /* srand, rand */
#include <fstream>
#include <string>

namespace Zoltan2{

/*! \brief A Dragonfly (e.g. Cori & Trinity) Machine Class for 
 *  testing only. A more realistic machine should be used for 
 *  task mapping.
 *
 *  Does NOT require RCA library to run. 
 */

template <typename pcoord_t, typename part_t>
class MachineDragonflyForTesting : public Machine <pcoord_t, part_t> {

public:
  /*! \brief Constructor: Dragonfly (e.g. Cori & Trinity) network 
   *  machine description;
   *
   *  Does not do coord transformation.
   *  
   *  \param comm Communication object.
   */

  MachineDragonflyForTesting(const Teuchos::Comm<int> &comm):
    Machine<pcoord_t,part_t>(comm),
    transformed_networkDim(3), 
    actual_networkDim(3),
    transformed_procCoords(NULL), 
    actual_procCoords(NULL),
    transformed_machine_extent(NULL),
    actual_machine_extent(NULL),
    is_transformed(false), 
    pl(NULL)
  {
    actual_machine_extent = new int[actual_networkDim];
    this->getActualMachineExtent(this->actual_machine_extent);
    
    // Transformed dims = 1 + N_y + N_z
    transformed_networkDim = 1 + actual_machine_extent[1] + 
      actual_machine_extent[2];
    actual_machine_extent = new int[transformed_networkDim];

    // Allocate memory for processor coords
    actual_procCoords = new pcoord_t *[actual_networkDim];
    transformed_procCoords = new pcoord_t *[transformed_networkDim];
    
    std::cout << "\nDef About to allocate Actual coord memory\n";
    
    for (int i = 0; i < actual_networkDim; ++i) {
      actual_procCoords[i] = new pcoord_t[this->numRanks];
      memset(actual_procCoords[i], 0, 
          sizeof(pcoord_t) * this->numRanks);
    }

    pcoord_t *xyz = new pcoord_t[transformed_networkDim];
    getMyActualMachineCoordinate(xyz);
    for (int i = 0; i < actual_networkDim; ++i)
      actual_procCoords[i][this->myRank] = xyz[i];
    delete [] xyz;

    std::cout << "\nDef About to gather coords\n";
    
    // reduceAll the coordinates of each processor.
    gatherMachineCoordinates(this->actual_procCoords,
        this->actual_networkDim, comm);

    std::cout << "\nDef gathered: " << actual_procCoords[1][0] << "\n";
  }

  // No necessary wrap arounds for dragonfly networks. Groups
  // have wrap around, but group all-to-all connection makes unneccessary.
  virtual bool getMachineExtentWrapArounds(bool *wrap_around) const {
    return false;
  }


 /*! \brief Constructor: Dragonfly (e.g. Cori & Trinity) network 
   *  machine description;
   *
   *  Does coord transformation if parameter list has a "Machine 
   *  Optimization Level" parameter set. 
   *  
   *  \param comm Communication object.
   *  \param pl   Parameter List
   */

  MachineDragonflyForTesting(const Teuchos::Comm<int> &comm, 
             const Teuchos::ParameterList &pl_ ):
    Machine<pcoord_t,part_t>(comm),
    transformed_networkDim(3), 
    actual_networkDim(3),
    transformed_procCoords(NULL), 
    actual_procCoords(NULL),
    transformed_machine_extent(NULL),
    actual_machine_extent(NULL),
    is_transformed(false), 
    pl(&pl_)
  {
    actual_machine_extent = new int[actual_networkDim];
    this->getActualMachineExtent(this->actual_machine_extent);
    
    // Allocate memory for processor coords
    actual_procCoords = new pcoord_t *[actual_networkDim];
    transformed_procCoords = new pcoord_t *[transformed_networkDim];
    
    pcoord_t *xyz = new pcoord_t[transformed_networkDim];
    getMyActualMachineCoordinate(xyz);

    const Teuchos::ParameterEntry *pe2 = 
      this->pl->getEntryPtr("Machine_Optimization_Level");

    std::cout << "\nRead PLIST\n";

    // Transform with mach opt level
    if (pe2) {
      int optimization_level;
      optimization_level = pe2->getValue<int>(&optimization_level);

      if (optimization_level > 0) {
        is_transformed = true;

        if (this->myRank == 0) 
          std::cout << "Transforming the coordinates" << std::endl;
       
        // Transformed dims = 1 + N_y + N_z
        transformed_networkDim = 1 + actual_machine_extent[1] + 
          actual_machine_extent[2];
        transformed_machine_extent = new int[transformed_networkDim];

        transformed_procCoords = new pcoord_t *[transformed_networkDim];

    std::cout << "\nAbout to allocate Trans coord memory\n";
    
        // Allocate memory for transformed coordinates
        for (int i = 0; i < transformed_networkDim; ++i) {
          transformed_procCoords[i] = new pcoord_t[this->numRanks];
          memset(transformed_procCoords[i], 0,
                 sizeof(pcoord_t) * this->numRanks);
        }
        
        // Calculate transformed coordinates and machine extents
        int nx = this->actual_machine_extent[0];
        int ny = this->actual_machine_extent[1];
        int nz = this->actual_machine_extent[2];
        
        transformed_procCoords[0][this->myRank] = 2 * xyz[0] * ny * nz;

        for (int i = 1; i < 1 + ny; ++i) {
          // Shift y-coord given a group, xyz[0];
          transformed_procCoords[i][this->myRank] = xyz[0] * ny * nz;
          // Increment in the dim where y-coord present  
          if (xyz[1] == i - 1)
            ++transformed_procCoords[i][this->myRank];
        }
        for (int i = 1 + ny; i < transformed_networkDim; ++i) {
          // Shift z-coord given a group, xyz[0];
          transformed_procCoords[0][this->myRank] = xyz[0] * ny * nz;
          // Increment in the dim where z-coord present
          if (xyz[2] == i - (1 + ny))
            ++transformed_procCoords[i][this->myRank];
        }

        this->transformed_machine_extent = new int[transformed_networkDim];
        
        // Max shifted high dim coordinate system
        this->transformed_machine_extent[0] = 2 * (nx - 1) * ny * nz;
        for (int i = 1; i < 1 + ny; ++i) {
          this->transformed_machine_extent[i] = 
            (ny - 1) + (nx - 1) * ny * nz;
        }
        for (int i = 1 + ny; i < transformed_networkDim; ++i) {
          this->transformed_machine_extent[i] = 
            (nz - 1) + (nx - 1) * ny * nz;
        }

    std::cout << "\nAbout to gather coords\n";
          
        // reduceAll the transformed coordinates of each processor.
        gatherMachineCoordinates(this->transformed_procCoords, 
          this->transformed_networkDim, comm);    
        
        if (this->myRank == 0) 
          std::cout << "Coordinates transformed!" << std::endl;
        this->printAllocation();
      }
    }
    // If no coordinate transformation, gather actual coords
    if (!is_transformed) {

      for (int i = 0; i < actual_networkDim; ++i) {
        actual_procCoords[i] = new pcoord_t[this->numRanks];
        memset(transformed_procCoords[i], 0, 
               sizeof(pcoord_t) * this->numRanks);
      }

      for (int i = 0; i < actual_networkDim; ++i)
        actual_procCoords[i][this->myRank] = xyz[i];

      // reduceAll the actual coordinates of each processor
      gatherMachineCoordinates(this->actual_procCoords,
          this->actual_networkDim, comm);

      this->printAllocation();
    }
    delete [] xyz;
  }

  virtual ~MachineDragonflyForTesting() {
    if (is_transformed) {
      is_transformed = false;
      for (int i = 0; i < transformed_networkDim; ++i) {
        delete [] transformed_procCoords[i];
      }
      delete [] transformed_procCoords;
      delete [] transformed_machine_extent;
    }
    for (int i = 0; i < actual_networkDim; ++i) {
      delete [] actual_procCoords[i];
    }
    delete [] actual_procCoords;
    delete [] actual_machine_extent;
  }

  bool hasMachineCoordinates() const { return true; }

  int getMachineDim() const {
    if (is_transformed) 
      return this->transformed_networkDim;
    else
      return this->actual_networkDim;
  }

  bool getTransformedMachineExtent(int *nxyz) const {
    if (is_transformed) {
      for (int dim = 0; dim < transformed_networkDim; ++dim)
        nxyz[dim] = this->transformed_machine_extent[dim];

      return true;
    }
    else 
      return false;
  }

  // Return RCA machine extents
  bool getActualMachineExtent(int *nxyz) const {
/*
#if defined (HAVE_ZOLTAN2_RCALIB)
    mesh_coord_t mxyz;
    rca_get_max_dimension(&mxyz);

    int dim = 0;
    nxyz[dim++] = mxyz.mesh_x + 1; // X - group [0, ~100]
    nxyz[dim++] = mxyz.mesh_y + 1; // Y - row within group [0, 5]
    nxyz[dim++] = mxyz.mesh_z + 1; // Z - col within row [0, 15]
    return true;
#else
    return false;
#endif
*/
    
    nxyz[0] = 11; // X - group
    nxyz[1] = 1;  // Y - row within group
    nxyz[2] = 1; // Z - col within group
   
    return true; 
  }

  bool getMachineExtent(int *nxyz) const {
    if (is_transformed) 
      this->getTransformedMachineExtent(nxyz);
    else
      this->getActualMachineExtent(nxyz);
    
    return true;
  }

  void printAllocation() {
    if (this->myRank == 0) {
      // Print transformed coordinates and extents
      if (is_transformed) {
        for (int i = 0; i < this->numRanks; ++i) { 
          std::cout << "Rank:" << i;
            for (int j = 0; j < this->transformed_networkDim; ++j) {
              std::cout << " " << transformed_procCoords[j][i]; 
            }
            std::cout << std::endl;  
        } 

        std::cout << std::endl << "Transformed Machine Extent: ";
        for (int i = 0; i < this->transformed_networkDim; ++i) {
          std::cout << " " << this->transformed_machine_extent[i];
        }
        std::cout << std::endl;
      }
      // Print actual coordinates and extents
      else {
        for (int i = 0; i < this->numRanks; ++i) { 
          std::cout << "Rank:" << i;
            for (int j = 0; j < this->actual_networkDim; ++j) {
              std::cout << " " << actual_procCoords[j][i]; 
            }
            std::cout << std::endl;  
        } 

        std::cout << std::endl << "Actual Machine Extent: ";
        for (int i = 0; i < this->actual_networkDim; ++i) {
          std::cout << " " << this->actual_machine_extent[i];
        }
        std::cout << std::endl;
      }
    }
  }

  bool getMyTransformedMachineCoordinate(pcoord_t *xyz) {
    if (is_transformed) {
      for (int i = 0; i < this->transformed_networkDim; ++i) {
        xyz[i] = transformed_procCoords[i][this->myRank];
      }

      return true;
    }
    else
      return false;
  }

  bool getMyActualMachineCoordinate(pcoord_t *xyz) {
/*
#if defined (HAVE_ZOLTAN2_RCALIB)
    // Cray node info for current node
    rs_node_t nodeInfo; 
    rca_get_nodeid(&nodeInfo);

    // Current node ID
    int NIDs = (int)nodeInfo.rs_node_s._node_id;

    mesh_coord_t node_coord;
    int returnval = rca_get_meshcoord((uint16_t)NIDs, &node_coord);
    if (returnval == -1) {
      return false;
    }
    xyz[0] = node_coord.mesh_x;
    xyz[1] = node_coord.mesh_y;
    xyz[2] = node_coord.mesh_z;
    return true;
#else
    return false;
#endif
*/
    srand(this->myRank);

    xyz[0] = rand() % 11;
    xyz[1] = rand() % 1;
    xyz[2] = rand() % 1;
 
    return true; 
  }

  bool getMyMachineCoordinate(pcoord_t *xyz) {
    if (is_transformed) 
      this->getMyTransformedMachineCoordinate(xyz);
    else
      this->getMyActualMachineCoordinate(xyz);
  
    return true;
  }

  inline bool getMachineCoordinate(const int rank,
                                   pcoord_t *xyz) const {
    if (is_transformed) {
      for (int i = 0; i < this->transformed_networkDim; ++i) {
        xyz[i] = transformed_procCoords[i][rank];
      }
    }
    else {
      for (int i = 0; i < this->actual_networkDim; ++i) {
        xyz[i] = actual_procCoords[i][rank];
      }
    }

    return true;   
  }

  bool getMachineCoordinate(const char *nodename, pcoord_t *xyz) {
    return false;  // cannot yet return from nodename
  }
 
  bool getAllMachineCoordinatesView(pcoord_t **&allCoords) const {
    if (is_transformed) {
      allCoords = transformed_procCoords; 
    }
    else {
      allCoords = actual_procCoords;
    }

    return true;
  }

  // Return (approx) hop count from rank1 to rank2. Does not account for 
  // dynamic routing.
  virtual bool getHopCount(int rank1, int rank2, pcoord_t &hops) {
    hops = 0;

    if (is_transformed) {     
      // Case: ranks in different groups
      // Does not account for location of group to group connection. 
      // (Most group to group messages will take 5 hops)
      if (transformed_procCoords[0][rank1] != 
          transformed_procCoords[0][rank2]) 
      {
        hops = 5;
        return true;
      }

      // Case: ranks in same group
      // For each 2 differences in transformed_coordinates then
      // 1 hop
      for (int i = 1; i < transformed_networkDim; ++i) {
        if (transformed_procCoords[i][rank1] != 
            transformed_procCoords[i][rank2]) 
          ++hops;
      }
      hops /= 2;
    }
    else {

      std::cout << "\nRank1: " << rank1
        << " Coords: " << actual_procCoords[0][rank1]
        << actual_procCoords[1][rank1]
        << actual_procCoords[2][rank1]
        << " Rank2: " << rank2 
        << " Coords: " << actual_procCoords[0][rank2]
        << actual_procCoords[1][rank2]
        << actual_procCoords[2][rank2];


      // Case: ranks in different groups
      // Does not account for location of group to group connection. 
      // (Nearly all group to group messages will take 5 hops)
      if (actual_procCoords[0][rank1] != 
          actual_procCoords[0][rank2]) 
      {
        hops = 5;

        std::cout << " Hops: 5\n";

        return true;
      }
      
      // Case: ranks in same group
      // For each difference in actual_coordinates then
      // 1 hop
      for (int i = 1; i < actual_networkDim; ++i) {
        if (actual_procCoords[i][rank1] != 
            actual_procCoords[i][rank2]) 
          ++hops;
      }
      std::cout << " Hops: " << hops << "\n";
    }

    return true;
  }

private:

  int transformed_networkDim;
  int actual_networkDim;

  pcoord_t **transformed_procCoords;
  pcoord_t **actual_procCoords;

  part_t *transformed_machine_extent;
  part_t *actual_machine_extent;
  bool is_transformed;

  const Teuchos::ParameterList *pl;
//  bool delete_tranformed_coords;

/*
  bool delete_transformed_coords;
  int transformed_network_dim;
  pcoord_t **transformed_coordinates;
*/

  void gatherMachineCoordinates(pcoord_t **&coords, int netDim, 
      const Teuchos::Comm<int> &comm) {
    // Reduces and stores all machine coordinates.
    pcoord_t *tmpVect = new pcoord_t [this->numRanks];

    for (int i = 0; i < netDim; ++i) {
      Teuchos::reduceAll<int, pcoord_t>(comm, Teuchos::REDUCE_SUM,
                                        this->numRanks, 
                                        coords[i], tmpVect);
      pcoord_t *tmp = tmpVect;
      tmpVect = coords[i];
      coords[i] = tmp;
    }
    delete [] tmpVect;
  }

};
}
#endif
