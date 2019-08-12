//@HEADER
// ************************************************************************
//
//               ShyLU: Hybrid preconditioner package
//                 Copyright 2012 Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Alexander Heinlein (alexander.heinlein@uni-koeln.de)
//
// ************************************************************************
//@HEADER

#ifndef _FROSCH_INTERFACEPARTITIONOFUNITY_DEF_HPP
#define _FROSCH_INTERFACEPARTITIONOFUNITY_DEF_HPP

#include <FROSch_InterfacePartitionOfUnity_decl.hpp>

namespace FROSch {

    template <class SC,class LO,class GO,class NO>
    InterfacePartitionOfUnity<SC,LO,GO,NO>::InterfacePartitionOfUnity(CommPtr mpiComm,
                                                                      CommPtr serialComm,
                                                                      UN dimension,
                                                                      UN dofsPerNode,
                                                                      MapPtr nodesMap,
                                                                      MapPtrVecPtr dofsMaps,
                                                                      ParameterListPtr parameterList,
                                                                      Verbosity verbosity) :
    MpiComm_ (mpiComm),
    SerialComm_ (serialComm),
    DDInterface_ (),
    ParameterList_ (parameterList),
    LocalPartitionOfUnity_ (),
    PartitionOfUnityMaps_ (),
    Verbose_ (MpiComm_->getRank() == 0)
    {
        CommunicationStrategy communicationStrategy;
        if (!ParameterList_->get("Interface Communication Strategy","CreateOneToOneMap").compare("Matrix")) {
            communicationStrategy = CommMatrix;
        } else if (!ParameterList_->get("Interface Communication Strategy","CreateOneToOneMap").compare("CrsGraph")) {
            communicationStrategy = CommGraph;
        } else if (!ParameterList_->get("Interface Communication Strategy","CreateOneToOneMap").compare("CreateOneToOneMap")) {
            communicationStrategy = CreateOneToOneMap;
        } else {
            FROSCH_ASSERT(false,"FROSch::InterfacePartitionOfUnity : ERROR: Specify a valid communication strategy for the identification of the interface components.");
        }

        DDInterface_.reset(new DDInterface<SC,LO,GO,NO>(dimension,dofsPerNode,nodesMap.getConst(),verbosity,communicationStrategy));

        DDInterface_->resetGlobalDofs(dofsMaps);
    }

    template <class SC,class LO,class GO,class NO>
    InterfacePartitionOfUnity<SC,LO,GO,NO>::~InterfacePartitionOfUnity()
    {

    }

    template <class SC,class LO,class GO,class NO>
    typename InterfacePartitionOfUnity<SC,LO,GO,NO>::MultiVectorPtrVecPtr InterfacePartitionOfUnity<SC,LO,GO,NO>::getLocalPartitionOfUnity() const
    {
        return LocalPartitionOfUnity_;
    }

    template <class SC,class LO,class GO,class NO>
    typename InterfacePartitionOfUnity<SC,LO,GO,NO>::MapPtrVecPtr InterfacePartitionOfUnity<SC,LO,GO,NO>::getPartitionOfUnityMaps() const
    {
        return PartitionOfUnityMaps_;
    }

    template <class SC,class LO,class GO,class NO>
    typename InterfacePartitionOfUnity<SC,LO,GO,NO>::ConstDDInterfacePtr InterfacePartitionOfUnity<SC,LO,GO,NO>::getDDInterface() const
    {
        return DDInterface_.getConst();
    }

}

#endif
