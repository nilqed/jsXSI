   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/06/16             */
   /*                                                     */
   /*                 DEFTEMPLATE MODULE                  */
   /*******************************************************/

/*************************************************************/
/* Purpose: Defines basic deftemplate primitive functions    */
/*   such as allocating and deallocating, traversing, and    */
/*   finding deftemplate data structures.                    */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Added support for templates maintaining their  */
/*            own list of facts.                             */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Added code for deftemplate run time            */
/*            initialization of hashed comparisons to        */
/*            constants.                                     */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Support for deftemplate slot facets.           */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>

#include "cstrccom.h"
#include "cstrnchk.h"
#include "envrnmnt.h"
#include "exprnops.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "network.h"
#include "router.h"
#include "tmpltbsc.h"
#include "tmpltfun.h"
#include "tmpltpsr.h"
#include "tmpltutl.h"

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "tmpltbin.h"
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "tmpltcmp.h"
#endif

#include "tmpltdef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                   *AllocateModule(Environment *);
   static void                    ReturnModule(Environment *,void *);
   static void                    ReturnDeftemplate(Environment *,Deftemplate *);
   static void                    InitializeDeftemplateModules(Environment *);
   static void                    DeallocateDeftemplateData(Environment *);
   static void                    DestroyDeftemplateAction(Environment *,struct constructHeader *,void *);
   static void                    DestroyDeftemplate(Environment *,Deftemplate *);
#if RUN_TIME
   static void                    RuntimeDeftemplateAction(Environment *,struct constructHeader *,void *);
   static void                    SearchForHashedPatternNodes(Environment *,struct factPatternNode *);
#endif

/******************************************************************/
/* InitializeDeftemplates: Initializes the deftemplate construct. */
/******************************************************************/
void InitializeDeftemplates(
  Environment *theEnv)
  {
   struct entityRecord deftemplatePtrRecord =
      { "DEFTEMPLATE_PTR",
        DEFTEMPLATE_PTR,1,0,0,
        NULL,
        NULL,NULL,
        NULL,
        NULL,
        (EntityBusyCountFunction *) DecrementDeftemplateBusyCount,
        (EntityBusyCountFunction *) IncrementDeftemplateBusyCount,
        NULL,NULL,NULL,NULL,NULL };
   AllocateEnvironmentData(theEnv,DEFTEMPLATE_DATA,sizeof(struct deftemplateData),DeallocateDeftemplateData);

   memcpy(&DeftemplateData(theEnv)->DeftemplatePtrRecord,&deftemplatePtrRecord,sizeof(struct entityRecord));   

   InitializeFacts(theEnv);

   InitializeDeftemplateModules(theEnv);

   DeftemplateBasicCommands(theEnv);

   DeftemplateFunctions(theEnv);

   DeftemplateData(theEnv)->DeftemplateConstruct =
      AddConstruct(theEnv,"deftemplate","deftemplates",ParseDeftemplate,
                   (FindConstructFunction *) EnvFindDeftemplate,
                   GetConstructNamePointer,GetConstructPPForm,
                   GetConstructModuleItem,
                   (GetNextConstructFunction *) EnvGetNextDeftemplate,
                   SetNextConstruct,
                   (IsConstructDeletableFunction *) EnvIsDeftemplateDeletable,
                   (DeleteConstructFunction *) EnvUndeftemplate,
                   (FreeConstructFunction *) ReturnDeftemplate);

   InstallPrimitive(theEnv,(ENTITY_RECORD_PTR) &DeftemplateData(theEnv)->DeftemplatePtrRecord,DEFTEMPLATE_PTR);
  }

/******************************************************/
/* DeallocateDeftemplateData: Deallocates environment */
/*    data for the deftemplate construct.             */
/******************************************************/
static void DeallocateDeftemplateData(
  Environment *theEnv)
  {
#if ! RUN_TIME
   struct deftemplateModule *theModuleItem;
   Defmodule *theModule;
#endif
#if BLOAD || BLOAD_AND_BSAVE
   if (Bloaded(theEnv)) return;
#endif

   DoForAllConstructs(theEnv,DestroyDeftemplateAction,DeftemplateData(theEnv)->DeftemplateModuleIndex,false,NULL);

#if ! RUN_TIME
   for (theModule = EnvGetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
     {
      theModuleItem = (struct deftemplateModule *)
                      GetModuleItem(theEnv,theModule,
                                    DeftemplateData(theEnv)->DeftemplateModuleIndex);
      rtn_struct(theEnv,deftemplateModule,theModuleItem);
     }
#endif
  }
  
/*****************************************************/
/* DestroyDeftemplateAction: Action used to remove   */
/*   deftemplates as a result of DestroyEnvironment. */
/*****************************************************/
static void DestroyDeftemplateAction(
  Environment *theEnv,
  struct constructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Deftemplate *theDeftemplate = (Deftemplate *) theConstruct;
   
   if (theDeftemplate == NULL) return;
   
   DestroyDeftemplate(theEnv,theDeftemplate);
  }


/*************************************************************/
/* InitializeDeftemplateModules: Initializes the deftemplate */
/*   construct for use with the defmodule construct.         */
/*************************************************************/
static void InitializeDeftemplateModules(
  Environment *theEnv)
  {
   DeftemplateData(theEnv)->DeftemplateModuleIndex = RegisterModuleItem(theEnv,"deftemplate",
                                    AllocateModule,
                                    ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
                                    BloadDeftemplateModuleReference,
#else
                                    NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
                                    DeftemplateCModuleReference,
#else
                                    NULL,
#endif
                                    (FindConstructFunction *) EnvFindDeftemplateInModule);

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT
   AddPortConstructItem(theEnv,"deftemplate",SYMBOL);
#endif
  }

/***************************************************/
/* AllocateModule: Allocates a deftemplate module. */
/***************************************************/
static void *AllocateModule(
  Environment *theEnv)
  {    
   return((void *) get_struct(theEnv,deftemplateModule)); 
  }

/*************************************************/
/* ReturnModule: Deallocates a deftemplate module. */
/*************************************************/
static void ReturnModule(
  Environment *theEnv,
  void *theItem)
  {   
   FreeConstructHeaderModule(theEnv,(struct defmoduleItemHeader *) theItem,DeftemplateData(theEnv)->DeftemplateConstruct);
   rtn_struct(theEnv,deftemplateModule,theItem);
  }

/****************************************************************/
/* GetDeftemplateModuleItem: Returns a pointer to the defmodule */
/*  item for the specified deftemplate or defmodule.            */
/****************************************************************/
struct deftemplateModule *GetDeftemplateModuleItem(
  Environment *theEnv,
  Defmodule *theModule)
  {   
   return((struct deftemplateModule *) GetConstructModuleItemByIndex(theEnv,theModule,DeftemplateData(theEnv)->DeftemplateModuleIndex)); 
  }

/*****************************************************/
/* EnvFindDeftemplate: Searches for a deftemplate in */
/*   the list of deftemplates. Returns a pointer to  */
/*   the deftemplate if  found, otherwise NULL.      */
/*****************************************************/
Deftemplate *EnvFindDeftemplate(
  Environment *theEnv,
  const char *deftemplateName)
  {  
   return(FindNamedConstructInModuleOrImports(theEnv,deftemplateName,DeftemplateData(theEnv)->DeftemplateConstruct)); 
  }

/*****************************************************/
/* EnvFindDeftemplateInModule: Searches for a deftemplate in */
/*   the list of deftemplates. Returns a pointer to  */
/*   the deftemplate if  found, otherwise NULL.      */
/*****************************************************/
Deftemplate *EnvFindDeftemplateInModule(
  Environment *theEnv,
  const char *deftemplateName)
  {  
   return(FindNamedConstructInModule(theEnv,deftemplateName,DeftemplateData(theEnv)->DeftemplateConstruct));
  }

/***********************************************************************/
/* EnvGetNextDeftemplate: If passed a NULL pointer, returns the first  */
/*   deftemplate in the ListOfDeftemplates. Otherwise returns the next */
/*   deftemplate following the deftemplate passed as an argument.      */
/***********************************************************************/
Deftemplate *EnvGetNextDeftemplate(
  Environment *theEnv,
  Deftemplate *deftemplatePtr)
  {   
   return((void *) GetNextConstructItem(theEnv,(struct constructHeader *) deftemplatePtr,DeftemplateData(theEnv)->DeftemplateModuleIndex)); 
  }

/***********************************************************/
/* EnvIsDeftemplateDeletable: Returns true if a particular */
/*   deftemplate can be deleted, otherwise returns false.  */
/***********************************************************/
bool EnvIsDeftemplateDeletable(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   if (! ConstructsDeletable(theEnv))
     { return false; }

   if (theDeftemplate->busyCount > 0) return false;
   if (theDeftemplate->patternNetwork != NULL) return false;

   return true;
  }

/**************************************************************/
/* ReturnDeftemplate: Returns the data structures associated  */
/*   with a deftemplate construct to the pool of free memory. */
/**************************************************************/
static void ReturnDeftemplate(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   struct templateSlot *slotPtr;

   if (theDeftemplate == NULL) return;

   /*====================================================================*/
   /* If a template is redefined, then we want to save its debug status. */
   /*====================================================================*/

#if DEBUGGING_FUNCTIONS
   DeftemplateData(theEnv)->DeletedTemplateDebugFlags = 0;
   if (theDeftemplate->watch) BitwiseSet(DeftemplateData(theEnv)->DeletedTemplateDebugFlags,0);
#endif

   /*===========================================*/
   /* Free storage used by the templates slots. */
   /*===========================================*/

   slotPtr = theDeftemplate->slotList;
   while (slotPtr != NULL)
     {
      DecrementSymbolCount(theEnv,slotPtr->slotName);
      RemoveHashedExpression(theEnv,slotPtr->defaultList);
      slotPtr->defaultList = NULL;
      RemoveHashedExpression(theEnv,slotPtr->facetList);
      slotPtr->facetList = NULL;
      RemoveConstraint(theEnv,slotPtr->constraints);
      slotPtr->constraints = NULL;
      slotPtr = slotPtr->next;
     }

   ReturnSlots(theEnv,theDeftemplate->slotList);

   /*==================================*/
   /* Free storage used by the header. */
   /*==================================*/

   DeinstallConstructHeader(theEnv,&theDeftemplate->header);

   rtn_struct(theEnv,deftemplate,theDeftemplate);
#endif
  }
  
/**************************************************************/
/* DestroyDeftemplate: Returns the data structures associated */
/*   with a deftemplate construct to the pool of free memory. */
/**************************************************************/
static void DestroyDeftemplate(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   struct templateSlot *slotPtr, *nextSlot;
#endif
   if (theDeftemplate == NULL) return;
  
#if (! BLOAD_ONLY) && (! RUN_TIME)
   slotPtr = theDeftemplate->slotList;

   while (slotPtr != NULL)
     {
      nextSlot = slotPtr->next;
      rtn_struct(theEnv,templateSlot,slotPtr);
      slotPtr = nextSlot;
     }
#endif

   DestroyFactPatternNetwork(theEnv,theDeftemplate->patternNetwork);
   
   /*==================================*/
   /* Free storage used by the header. */
   /*==================================*/

#if (! BLOAD_ONLY) && (! RUN_TIME)
   DeinstallConstructHeader(theEnv,&theDeftemplate->header);

   rtn_struct(theEnv,deftemplate,theDeftemplate);
#endif
  }
  
/***********************************************/
/* ReturnSlots: Returns the slot structures of */
/*   a deftemplate to free memory.             */
/***********************************************/
void ReturnSlots(
  Environment *theEnv,
  struct templateSlot *slotPtr)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   struct templateSlot *nextSlot;

   while (slotPtr != NULL)
     {
      nextSlot = slotPtr->next;
      ReturnExpression(theEnv,slotPtr->defaultList);
      ReturnExpression(theEnv,slotPtr->facetList);
      RemoveConstraint(theEnv,slotPtr->constraints);
      rtn_struct(theEnv,templateSlot,slotPtr);
      slotPtr = nextSlot;
     }
#endif
  }

/*************************************************/
/* DecrementDeftemplateBusyCount: Decrements the */
/*   busy count of a deftemplate data structure. */
/*************************************************/
void DecrementDeftemplateBusyCount(
  Environment *theEnv,
  Deftemplate *theTemplate)
  {
   if (! ConstructData(theEnv)->ClearInProgress) theTemplate->busyCount--;
  }

/*************************************************/
/* IncrementDeftemplateBusyCount: Increments the */
/*   busy count of a deftemplate data structure. */
/*************************************************/
void IncrementDeftemplateBusyCount(
  Environment *theEnv,
  Deftemplate *theTemplate)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif

   theTemplate->busyCount++;
  }
  
/*******************************************************************/
/* EnvGetNextFactInTemplate: If passed a NULL pointer, returns the */
/*   first fact in the template's fact-list. Otherwise returns the */
/*   next template fact following the fact passed as an argument.  */
/*******************************************************************/
Fact *EnvGetNextFactInTemplate(
  Environment *theEnv,
  Deftemplate *theTemplate,
  Fact *factPtr)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif
   if (factPtr == NULL)
     { return(theTemplate->factList); }

   if (factPtr->garbage) return NULL;

   return(factPtr->nextTemplateFact);
  }

#if ! RUN_TIME

/******************************/
/* CreateDeftemplateScopeMap: */
/******************************/
void *CreateDeftemplateScopeMap(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   unsigned scopeMapSize;
   char *scopeMap;
   const char *templateName;
   Defmodule *matchModule, *theModule;
   int moduleID,count;
   void *theBitMap;

   templateName = ValueToString(theDeftemplate->header.name);
   matchModule = theDeftemplate->header.whichModule->theModule;

   scopeMapSize = (sizeof(char) * ((GetNumberOfDefmodules(theEnv) / BITS_PER_BYTE) + 1));
   scopeMap = (char *) gm2(theEnv,scopeMapSize);

   ClearBitString((void *) scopeMap,scopeMapSize);
   SaveCurrentModule(theEnv);
   for (theModule = EnvGetNextDefmodule(theEnv,NULL) ;
        theModule != NULL ;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
     {
      EnvSetCurrentModule(theEnv,theModule);
      moduleID = (int) theModule->bsaveID;
      if (FindImportedConstruct(theEnv,"deftemplate",matchModule,
                                templateName,&count,true,NULL) != NULL)
        SetBitMap(scopeMap,moduleID);
     }
   RestoreCurrentModule(theEnv);
   theBitMap = EnvAddBitMap(theEnv,scopeMap,scopeMapSize);
   IncrementBitMapCount(theBitMap);
   rm(theEnv,scopeMap,scopeMapSize);
   return(theBitMap);
  }

#endif

#if RUN_TIME

/**************************************************/
/* RuntimeDeftemplateAction: Action to be applied */
/*   to each deftemplate construct when a runtime */
/*   initialization occurs.                       */
/**************************************************/
static void RuntimeDeftemplateAction(
  Environment *theEnv,
  struct constructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
   Deftemplate *theDeftemplate = (Deftemplate *) theConstruct;
   
   SearchForHashedPatternNodes(theEnv,theDeftemplate->patternNetwork);
  }

/********************************/
/* SearchForHashedPatternNodes: */
/********************************/
static void SearchForHashedPatternNodes(
   Environment *theEnv,
   struct factPatternNode *theNode)
   {
    while (theNode != NULL)
      {
       if ((theNode->lastLevel != NULL) && (theNode->lastLevel->header.selector))
        { AddHashedPatternNode(theEnv,theNode->lastLevel,theNode,theNode->networkTest->type,theNode->networkTest->value); }

       SearchForHashedPatternNodes(theEnv,theNode->nextLevel);
      
       theNode = theNode->rightNode;
      }
   }

/*********************************/
/* DeftemplateRunTimeInitialize: */
/*********************************/
void DeftemplateRunTimeInitialize(
  Environment *theEnv)
  {
   DoForAllConstructs(theEnv,RuntimeDeftemplateAction,DeftemplateData(theEnv)->DeftemplateModuleIndex,true,NULL);
  }

#endif /* RUN_TIME */

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *EnvDeftemplateModule(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   return GetConstructModuleName((struct constructHeader *) theDeftemplate);
  }

const char *EnvGetDeftemplateName(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   return GetConstructNameString((struct constructHeader *) theDeftemplate);
  }

const char *EnvGetDeftemplatePPForm(
  Environment *theEnv,
  Deftemplate *theDeftemplate)
  {
   return GetConstructPPForm(theEnv,(struct constructHeader *) theDeftemplate);
  }

#endif /* DEFTEMPLATE_CONSTRUCT */


