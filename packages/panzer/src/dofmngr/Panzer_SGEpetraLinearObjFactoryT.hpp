#ifdef HAVE_STOKHOS

#include "EpetraExt_BlockUtility.h"
#include "Epetra_LocalMap.h"

namespace panzer {

template <typename Traits,typename LocalOrdinalT>
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::SGEpetraLinearObjFactory(const Teuchos::RCP<EpetraLinearObjFactory<Traits,LocalOrdinalT> > & epetraFact,
                           const Teuchos::RCP<Stokhos::OrthogPolyExpansion<int,double> > & expansion,
                           const Teuchos::RCP<const EpetraExt::MultiComm> & globalMultiComm)
   : epetraFact_(epetraFact), expansion_(expansion), globalMultiComm_(globalMultiComm)
{
   // build and register the gather/scatter evaluators with 
   // the base class.
   buildGatherScatterEvaluators(*this);
}

template <typename Traits,typename LocalOrdinalT>
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::~SGEpetraLinearObjFactory() 
{
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<LinearObjContainer> 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::buildLinearObjContainer() const
{
   SGEpetraLinearObjContainer::CoeffVector coeffContainers;
   for(int i=0;i<expansion_->size();i++) {
      Teuchos::RCP<EpetraLinearObjContainer> eCont = 
         Teuchos::rcp_dynamic_cast<EpetraLinearObjContainer>(buildPrimitiveLinearObjContainer());
      coeffContainers.push_back(eCont);
   }

   return Teuchos::rcp(new SGEpetraLinearObjContainer(coeffContainers,expansion_));
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<LinearObjContainer> 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::buildPrimitiveLinearObjContainer() const
{
   return epetraFact_->buildLinearObjContainer();
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<LinearObjContainer> 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::buildGhostedLinearObjContainer() const
{
   SGEpetraLinearObjContainer::CoeffVector coeffContainers;
   for(int i=0;i<expansion_->size();i++) {
      Teuchos::RCP<EpetraLinearObjContainer> eCont = 
         Teuchos::rcp_dynamic_cast<EpetraLinearObjContainer>(buildPrimitiveGhostedLinearObjContainer());
      coeffContainers.push_back(eCont);
   }

   return Teuchos::rcp(new SGEpetraLinearObjContainer(coeffContainers,expansion_));
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<LinearObjContainer> 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::buildPrimitiveGhostedLinearObjContainer() const
{
   return epetraFact_->buildGhostedLinearObjContainer();
}

template <typename Traits,typename LocalOrdinalT>
void 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::globalToGhostContainer(const LinearObjContainer & container,LinearObjContainer & ghostContainer,int mem) const
{
   bool completed = false;
   try {
      const SGEpetraLinearObjContainer & containerSG = Teuchos::dyn_cast<const SGEpetraLinearObjContainer>(container);
      SGEpetraLinearObjContainer & ghostContainerSG = Teuchos::dyn_cast<SGEpetraLinearObjContainer>(ghostContainer);
   
      // simply iterate over each deterministic system and run global to ghost
      SGEpetraLinearObjContainer::const_iterator inItr;
      SGEpetraLinearObjContainer::iterator outItr;
      for(inItr=containerSG.begin(),outItr=ghostContainerSG.begin();
          inItr!=containerSG.end();inItr++,outItr++) {
         epetraFact_->globalToGhostContainer(**inItr,**outItr,mem);
      }

      completed = true;
   }
   catch(const std::bad_cast & bad_cast) { }

   // type was not a SGEpetraLinearObjContainer, try the Epetra type
   if(!completed) {
      // this had many perils, primarily that the exception will come
      // from the epetra factory.
      epetraFact_->globalToGhostContainer(container,ghostContainer,mem);
   }
}

template <typename Traits,typename LocalOrdinalT>
void 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::ghostToGlobalContainer(const LinearObjContainer & ghostContainer, LinearObjContainer & container,int mem) const
{
   bool completed = false;
   try {
      SGEpetraLinearObjContainer & containerSG = Teuchos::dyn_cast<SGEpetraLinearObjContainer>(container);
      const SGEpetraLinearObjContainer & ghostContainerSG = Teuchos::dyn_cast<const SGEpetraLinearObjContainer>(ghostContainer);
   
      // simply iterate over each deterministic system and run ghost to global
      SGEpetraLinearObjContainer::const_iterator inItr;
      SGEpetraLinearObjContainer::iterator outItr;
      for(inItr=ghostContainerSG.begin(),outItr=containerSG.begin();
          inItr!=ghostContainerSG.end();inItr++,outItr++) {
         epetraFact_->ghostToGlobalContainer(**inItr,**outItr,mem);
      }

      completed = true;
   }
   catch(const std::bad_cast & bad_cast) { }

   // type was not a SGEpetraLinearObjContainer, try the Epetra type
   if(!completed) {
      // this had many perils, primarily that the exception will come
      // from the epetra factory.
      epetraFact_->ghostToGlobalContainer(ghostContainer,container,mem);
   }
}

template <typename Traits,typename LocalOrdinalT>
void SGEpetraLinearObjFactory<Traits,LocalOrdinalT>::
adjustForDirichletConditions(const LinearObjContainer & localBCRows,
                             const LinearObjContainer & globalBCRows,
                             LinearObjContainer & ghostedObjs) const
{
   bool completed = false;
   try {
      SGEpetraLinearObjContainer & ghostContainerSG = Teuchos::dyn_cast<SGEpetraLinearObjContainer>(ghostedObjs);
   
      // simply iterate over each deterministic system and run adjustForDirichlet
      SGEpetraLinearObjContainer::iterator ghostObjsItr;
      for(ghostObjsItr=ghostContainerSG.begin();ghostObjsItr!=ghostContainerSG.end();ghostObjsItr++)
         epetraFact_->adjustForDirichletConditions(localBCRows,globalBCRows,**ghostObjsItr);

      completed = true;
   }
   catch(const std::bad_cast & bad_cast) { }

   // type was not a SGEpetraLinearObjContainer, try the Epetra type
   if(!completed) {
      // this had many perils, primarily that the exception will come
      // from the epetra factory.
      epetraFact_->adjustForDirichletConditions(localBCRows,globalBCRows,ghostedObjs);
   }
}

template <typename Traits,typename LocalOrdinalT>
void SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::initializeContainer(int mem,LinearObjContainer & loc) const
{
   bool completed = false;
   try {
      SGEpetraLinearObjContainer & eloc = Teuchos::dyn_cast<SGEpetraLinearObjContainer>(loc);
   
      SGEpetraLinearObjContainer::iterator itr;
      for(itr=eloc.begin();itr!=eloc.end();++itr) 
         epetraFact_->initializeContainer(mem,**itr);

      completed = true;
   }
   catch(const std::bad_cast & bad_cast) { }

   // type was not a SGEpetraLinearObjContainer, try the Epetra type
   if(!completed) {
      // this has many perils, primarily that the exception will come
      // from the epetra factory.
      epetraFact_->initializeContainer(mem,loc);
   }
}

template <typename Traits,typename LocalOrdinalT>
void 
SGEpetraLinearObjFactory<Traits,LocalOrdinalT>
::initializeGhostedContainer(int mem,LinearObjContainer & loc) const
{
   bool completed = false;
   try {
      SGEpetraLinearObjContainer & eloc = Teuchos::dyn_cast<SGEpetraLinearObjContainer>(loc);

      SGEpetraLinearObjContainer::iterator itr;
      for(itr=eloc.begin();itr!=eloc.end();++itr) 
         epetraFact_->initializeGhostedContainer(mem,**itr);

      completed = true;
   }
   catch(const std::bad_cast & bad_cast) { }

   // type was not a SGEpetraLinearObjContainer, try the Epetra type
   if(!completed) {
      // this has many perils, primarily that the exception will come
      // from the epetra factory.
      epetraFact_->initializeGhostedContainer(mem,loc);
   }
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<Stokhos::EpetraVectorOrthogPoly> SGEpetraLinearObjFactory<Traits,LocalOrdinalT>::
getVectorOrthogPoly() const
{
   Teuchos::RCP<const Epetra_Map> blockMap = getSGBlockMap();
   Teuchos::RCP<Epetra_Map> epMap = epetraFact_->getMap();
   return Teuchos::rcp(new Stokhos::EpetraVectorOrthogPoly(expansion_->getBasis(),blockMap,epMap,globalMultiComm_));
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<const Epetra_Map> SGEpetraLinearObjFactory<Traits,LocalOrdinalT>::
getMap()
{
   Teuchos::RCP<const Epetra_Map> blockMap = getSGBlockMap();
   Teuchos::RCP<Epetra_Map> epMap = epetraFact_->getMap();
   return Teuchos::rcp(EpetraExt::BlockUtility::GenerateBlockMap(*epMap,*blockMap,*epetraFact_->getEpetraComm()));
}

template <typename Traits,typename LocalOrdinalT>
Teuchos::RCP<const Epetra_Map> SGEpetraLinearObjFactory<Traits,LocalOrdinalT>::
getSGBlockMap() const
{
   if(sgBlockMap_==Teuchos::null)
      sgBlockMap_ = Teuchos::rcp(new Epetra_LocalMap(expansion_->getBasis()->size(), 0, *epetraFact_->getEpetraComm()));
   return sgBlockMap_; 
}

}

#endif
