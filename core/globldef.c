   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*                  DEFGLOBAL MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for the creation and      */
/*   maintenance of the defglobal construct.                 */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Corrected code to remove run-time program      */
/*            compiler warning.                              */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Changed garbage collection algorithm.          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Fixed linkage issue when BLOAD_ONLY compiler   */
/*            flag is set to 1.                              */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT

#include <stdio.h>

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#include "globlbin.h"
#endif
#include "commline.h"
#include "envrnmnt.h"
#include "globlbsc.h"
#if CONSTRUCT_COMPILER && (! RUN_TIME)
#include "globlcmp.h"
#endif
#include "globlcom.h"
#include "globlpsr.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "multifld.h"
#include "router.h"
#include "strngrtr.h"
#include "utility.h"

#include "globldef.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                   *AllocateModule(Environment *);
   static void                    ReturnModule(Environment *,void *);
   static void                    ReturnDefglobal(Environment *,Defglobal *);
   static void                    InitializeDefglobalModules(Environment *);
   static bool                    GetDefglobalValue2(Environment *,void *,CLIPSValue *);
   static void                    IncrementDefglobalBusyCount(Environment *,Defglobal *);
   static void                    DecrementDefglobalBusyCount(Environment *,Defglobal *);
   static void                    DeallocateDefglobalData(Environment *);
   static void                    DestroyDefglobalAction(Environment *,struct constructHeader *,void *);
#if (! BLOAD_ONLY)
   static void                    DestroyDefglobal(Environment *,Defglobal *);
#endif

/**************************************************************/
/* InitializeDefglobals: Initializes the defglobal construct. */
/**************************************************************/
void InitializeDefglobals(
  Environment *theEnv)
  {  
   struct entityRecord globalInfo = { "GBL_VARIABLE", GBL_VARIABLE,0,0,0,
                                                       NULL,
                                                       NULL,
                                                       NULL,
                                                       (EntityEvaluationFunction *)  GetDefglobalValue2,
                                                       NULL,NULL,
                                                       NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord defglobalPtrRecord = { "DEFGLOBAL_PTR", DEFGLOBAL_PTR,0,0,0,
                                                       NULL,NULL,NULL,
                                                       (EntityEvaluationFunction *) QGetDefglobalValue,
                                                       NULL,
                                                       (EntityBusyCountFunction *) DecrementDefglobalBusyCount,
                                                       (EntityBusyCountFunction *) IncrementDefglobalBusyCount,
                                                       NULL,NULL,NULL,NULL,NULL };
   
   AllocateEnvironmentData(theEnv,DEFGLOBAL_DATA,sizeof(struct defglobalData),DeallocateDefglobalData);
   
   memcpy(&DefglobalData(theEnv)->GlobalInfo,&globalInfo,sizeof(struct entityRecord));   
   memcpy(&DefglobalData(theEnv)->DefglobalPtrRecord,&defglobalPtrRecord,sizeof(struct entityRecord));   

   DefglobalData(theEnv)->ResetGlobals = true;
   DefglobalData(theEnv)->LastModuleIndex = -1;
   
   InstallPrimitive(theEnv,&DefglobalData(theEnv)->GlobalInfo,GBL_VARIABLE);
   InstallPrimitive(theEnv,&DefglobalData(theEnv)->DefglobalPtrRecord,DEFGLOBAL_PTR);

   InitializeDefglobalModules(theEnv);

   DefglobalBasicCommands(theEnv);
   DefglobalCommandDefinitions(theEnv);

   DefglobalData(theEnv)->DefglobalConstruct =
      AddConstruct(theEnv,"defglobal","defglobals",ParseDefglobal,
                   (FindConstructFunction *) EnvFindDefglobal,
                   GetConstructNamePointer,GetConstructPPForm,
                   GetConstructModuleItem,
                   (GetNextConstructFunction *) EnvGetNextDefglobal,
                   SetNextConstruct,
                   (IsConstructDeletableFunction *) EnvIsDefglobalDeletable,
                   (DeleteConstructFunction *) EnvUndefglobal,
                   (FreeConstructFunction *) ReturnDefglobal);
  }

/****************************************************/
/* DeallocateDefglobalData: Deallocates environment */
/*    data for the defglobal construct.             */
/****************************************************/
static void DeallocateDefglobalData(
  Environment *theEnv)
  {
#if ! RUN_TIME
   struct defglobalModule *theModuleItem;
   Defmodule *theModule;
   
#if BLOAD || BLOAD_AND_BSAVE
   if (Bloaded(theEnv)) return;
#endif

   DoForAllConstructs(theEnv,DestroyDefglobalAction,
                      DefglobalData(theEnv)->DefglobalModuleIndex,false,NULL);

   for (theModule = EnvGetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
     {
      theModuleItem = (struct defglobalModule *)
                      GetModuleItem(theEnv,theModule,
                                    DefglobalData(theEnv)->DefglobalModuleIndex);
      rtn_struct(theEnv,defglobalModule,theModuleItem);
     }
#else
   DoForAllConstructs(theEnv,DestroyDefglobalAction,DefglobalData(theEnv)->DefglobalModuleIndex,false,NULL);
#endif
  }
  
/***************************************************/
/* DestroyDefglobalAction: Action used to remove   */
/*   defglobals as a result of DestroyEnvironment. */
/***************************************************/
static void DestroyDefglobalAction(
  Environment *theEnv,
  struct constructHeader *theConstruct,
  void *buffer)
  {
#if MAC_XCD
#pragma unused(buffer)
#endif
#if (! BLOAD_ONLY)
   Defglobal *theDefglobal = (Defglobal *) theConstruct;
   
   if (theDefglobal == NULL) return;

   DestroyDefglobal(theEnv,theDefglobal);
#else
#if MAC_XCD
#pragma unused(theEnv,theConstruct)
#endif
#endif
  }

/*********************************************************/
/* InitializeDefglobalModules: Initializes the defglobal */
/*   construct for use with the defmodule construct.     */
/*********************************************************/
static void InitializeDefglobalModules(
  Environment *theEnv)
  {
   DefglobalData(theEnv)->DefglobalModuleIndex = RegisterModuleItem(theEnv,"defglobal",
                                    AllocateModule,
                                    ReturnModule,
#if BLOAD_AND_BSAVE || BLOAD || BLOAD_ONLY
                                    BloadDefglobalModuleReference,
#else
                                    NULL,
#endif
#if CONSTRUCT_COMPILER && (! RUN_TIME)
                                    DefglobalCModuleReference,
#else
                                    NULL,
#endif
                                    (FindConstructFunction *) EnvFindDefglobalInModule);

#if (! BLOAD_ONLY) && (! RUN_TIME) && DEFMODULE_CONSTRUCT
   AddPortConstructItem(theEnv,"defglobal",SYMBOL);
#endif
  }

/*************************************************/
/* AllocateModule: Allocates a defglobal module. */
/*************************************************/
static void *AllocateModule(
  Environment *theEnv)
  {   
   return (void *) get_struct(theEnv,defglobalModule);
  }

/*************************************************/
/* ReturnModule: Deallocates a defglobal module. */
/*************************************************/
static void ReturnModule(
  Environment *theEnv,
  void *theItem)
  {
   FreeConstructHeaderModule(theEnv,(struct defmoduleItemHeader *) theItem,DefglobalData(theEnv)->DefglobalConstruct);
   rtn_struct(theEnv,defglobalModule,theItem);
  }

/**************************************************************/
/* GetDefglobalModuleItem: Returns a pointer to the defmodule */
/*  item for the specified defglobal or defmodule.            */
/**************************************************************/
struct defglobalModule *GetDefglobalModuleItem(
  Environment *theEnv,
  Defmodule *theModule)
  {
   return((struct defglobalModule *) GetConstructModuleItemByIndex(theEnv,theModule,DefglobalData(theEnv)->DefglobalModuleIndex));
  }

/*****************************************************/
/* EnvFindDefglobal: Searches for a defglobal in the */
/*   list of defglobals. Returns a pointer to the    */
/*   defglobal if found, otherwise NULL.             */
/*****************************************************/
Defglobal *EnvFindDefglobal(
  Environment *theEnv,
  const char *defglobalName)
  { 
   return(FindNamedConstructInModuleOrImports(theEnv,defglobalName,DefglobalData(theEnv)->DefglobalConstruct)); 
  }

/*****************************************************/
/* EnvFindDefglobalInModule: Searches for a defglobal in the */
/*   list of defglobals. Returns a pointer to the    */
/*   defglobal if found, otherwise NULL.             */
/*****************************************************/
Defglobal *EnvFindDefglobalInModule(
  Environment *theEnv,
  const char *defglobalName)
  { 
   return(FindNamedConstructInModule(theEnv,defglobalName,DefglobalData(theEnv)->DefglobalConstruct)); 
  }

/********************************************************************/
/* EnvGetNextDefglobal: If passed a NULL pointer, returns the first */
/*   defglobal in the defglobal list. Otherwise returns the next    */
/*   defglobal following the defglobal passed as an argument.       */
/********************************************************************/
Defglobal *EnvGetNextDefglobal(
  Environment *theEnv,
  Defglobal *defglobalPtr)
  { 
   return (Defglobal *) GetNextConstructItem(theEnv,(struct constructHeader *) defglobalPtr,DefglobalData(theEnv)->DefglobalModuleIndex);
  }

/*********************************************************/
/* EnvIsDefglobalDeletable: Returns true if a particular */
/*   defglobal can be deleted, otherwise returns false.  */
/*********************************************************/
bool EnvIsDefglobalDeletable(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
   if (! ConstructsDeletable(theEnv))
     { return false; }

   if (theDefglobal->busyCount) return false;

   return true;
  }

/************************************************************/
/* ReturnDefglobal: Returns the data structures associated  */
/*   with a defglobal construct to the pool of free memory. */
/************************************************************/
static void ReturnDefglobal(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
#if (! BLOAD_ONLY) && (! RUN_TIME)
   if (theDefglobal == NULL) return;

   /*====================================*/
   /* Return the global's current value. */
   /*====================================*/

   ValueDeinstall(theEnv,&theDefglobal->current);
   if (theDefglobal->current.type == MULTIFIELD)
     { ReturnMultifield(theEnv,(struct multifield *) theDefglobal->current.value); }

   /*================================================*/
   /* Return the expression representing the initial */
   /* value of the defglobal when it was defined.    */
   /*================================================*/

   RemoveHashedExpression(theEnv,theDefglobal->initial);

   /*===============================*/
   /* Release items stored in the   */
   /* defglobal's construct header. */
   /*===============================*/

   DeinstallConstructHeader(theEnv,&theDefglobal->header);

   /*======================================*/
   /* Return the defglobal data structure. */
   /*======================================*/

   rtn_struct(theEnv,defglobal,theDefglobal);

   /*===========================================*/
   /* Set the variable indicating that a change */
   /* has been made to a global variable.       */
   /*===========================================*/

   DefglobalData(theEnv)->ChangeToGlobals = true;
#endif
  }
  
/************************************************************/
/* DestroyDefglobal: Returns the data structures associated  */
/*   with a defglobal construct to the pool of free memory. */
/************************************************************/
#if (! BLOAD_ONLY)
static void DestroyDefglobal(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
   if (theDefglobal == NULL) return;

   /*====================================*/
   /* Return the global's current value. */
   /*====================================*/

   if (theDefglobal->current.type == MULTIFIELD)
     { ReturnMultifield(theEnv,(struct multifield *) theDefglobal->current.value); }
     
#if (! RUN_TIME)

   /*===============================*/
   /* Release items stored in the   */
   /* defglobal's construct header. */
   /*===============================*/

   DeinstallConstructHeader(theEnv,&theDefglobal->header);

   /*======================================*/
   /* Return the defglobal data structure. */
   /*======================================*/

   rtn_struct(theEnv,defglobal,theDefglobal);
#endif
  }
#endif

/************************************************/
/* QSetDefglobalValue: Lowest level routine for */
/*   setting a defglobal's value.               */
/************************************************/
void QSetDefglobalValue(
  Environment *theEnv,
  Defglobal *theGlobal,
  CLIPSValue *vPtr,
  bool resetVar)
  {
   /*====================================================*/
   /* If the new value passed for the defglobal is NULL, */
   /* then reset the defglobal to the initial value it   */
   /* had when it was defined.                           */
   /*====================================================*/

   if (resetVar)
     {
      EvaluateExpression(theEnv,theGlobal->initial,vPtr);
      if (EvaluationData(theEnv)->EvaluationError)
        {
         vPtr->type = SYMBOL;
         vPtr->value = EnvFalseSymbol(theEnv);
        }
     }

   /*==========================================*/
   /* If globals are being watch, then display */
   /* the change to the global variable.       */
   /*==========================================*/

#if DEBUGGING_FUNCTIONS
   if (theGlobal->watch)
     {
      EnvPrintRouter(theEnv,WTRACE,":== ?*");
      EnvPrintRouter(theEnv,WTRACE,ValueToString(theGlobal->header.name));
      EnvPrintRouter(theEnv,WTRACE,"* ==> ");
      PrintDataObject(theEnv,WTRACE,vPtr);
      EnvPrintRouter(theEnv,WTRACE," <== ");
      PrintDataObject(theEnv,WTRACE,&theGlobal->current);
      EnvPrintRouter(theEnv,WTRACE,"\n");
     }
#endif

   /*==============================================*/
   /* Remove the old value of the global variable. */
   /*==============================================*/

   ValueDeinstall(theEnv,&theGlobal->current);
   if (theGlobal->current.type == MULTIFIELD)
     { ReturnMultifield(theEnv,(struct multifield *) theGlobal->current.value); }

   /*===========================================*/
   /* Set the new value of the global variable. */
   /*===========================================*/

   theGlobal->current.type = vPtr->type;
   if (vPtr->type != MULTIFIELD) theGlobal->current.value = vPtr->value;
   else DuplicateMultifield(theEnv,&theGlobal->current,vPtr);
   ValueInstall(theEnv,&theGlobal->current);

   /*===========================================*/
   /* Set the variable indicating that a change */
   /* has been made to a global variable.       */
   /*===========================================*/

   DefglobalData(theEnv)->ChangeToGlobals = true;

   if ((UtilityData(theEnv)->CurrentGarbageFrame->topLevel) && (! CommandLineData(theEnv)->EvaluatingTopLevelCommand) &&
       (EvaluationData(theEnv)->CurrentExpression == NULL) && (UtilityData(theEnv)->GarbageCollectionLocks == 0))
     {
      CleanCurrentGarbageFrame(theEnv,NULL);
      CallPeriodicTasks(theEnv);
     }
  }

/**************************************************************/
/* QFindDefglobal: Searches for a defglobal in the list of    */
/*   defglobals. Returns a pointer to the defglobal if found, */
/*   otherwise NULL.                                          */
/**************************************************************/
Defglobal *QFindDefglobal(
  Environment *theEnv,
  SYMBOL_HN *defglobalName)
  {
   Defglobal *theDefglobal;

   for (theDefglobal = EnvGetNextDefglobal(theEnv,NULL);
        theDefglobal != NULL;
        theDefglobal = EnvGetNextDefglobal(theEnv,theDefglobal))
     { if (defglobalName == theDefglobal->header.name) return (theDefglobal); }

   return NULL;
  }

/*********************************************************************/
/* EnvGetDefglobalValueForm: Returns the pretty print representation */
/*   of the current value of the specified defglobal. For example,   */
/*   if the current value of ?*x* is 5, the string "?*x* = 5" would  */
/*   be returned.                                                    */
/*********************************************************************/
void EnvGetDefglobalValueForm(
  Environment *theEnv,
  char *buffer,
  size_t bufferLength,
  Defglobal *theGlobal)
  {
   OpenStringDestination(theEnv,"GlobalValueForm",buffer,bufferLength);
   EnvPrintRouter(theEnv,"GlobalValueForm","?*");
   EnvPrintRouter(theEnv,"GlobalValueForm",ValueToString(theGlobal->header.name));
   EnvPrintRouter(theEnv,"GlobalValueForm","* = ");
   PrintDataObject(theEnv,"GlobalValueForm",&theGlobal->current);
   CloseStringDestination(theEnv,"GlobalValueForm");
  }

/************************************************************/
/* EnvGetGlobalsChanged: Returns the defglobal change flag. */
/************************************************************/
bool EnvGetGlobalsChanged(
  Environment *theEnv)
  {    
   return(DefglobalData(theEnv)->ChangeToGlobals); 
  }

/*********************************************************/
/* EnvSetGlobalsChanged: Sets the defglobal change flag. */
/*********************************************************/
void EnvSetGlobalsChanged(
  Environment *theEnv,
  bool value)
  {
   DefglobalData(theEnv)->ChangeToGlobals = value; 
  }

/**********************************************************/
/* GetDefglobalValue2: Returns the value of the specified */
/*   global variable in the supplied CLIPSValue.          */
/**********************************************************/
static bool GetDefglobalValue2(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *vPtr)
  {
   Defglobal *theGlobal;
   int count;

   /*===========================================*/
   /* Search for the specified defglobal in the */
   /* modules visible to the current module.    */
   /*===========================================*/

   theGlobal = (Defglobal *)
               FindImportedConstruct(theEnv,"defglobal",NULL,ValueToString(theValue),
               &count,true,NULL);

   /*=============================================*/
   /* If it wasn't found, print an error message. */
   /*=============================================*/

   if (theGlobal == NULL)
     {
      PrintErrorID(theEnv,"GLOBLDEF",1,false);
      EnvPrintRouter(theEnv,WERROR,"Global variable ?*");
      EnvPrintRouter(theEnv,WERROR,ValueToString(theValue));
      EnvPrintRouter(theEnv,WERROR,"* is unbound.\n");
      vPtr->type = SYMBOL;
      vPtr->value = EnvFalseSymbol(theEnv);
      EnvSetEvaluationError(theEnv,true);
      return false;
     }

   /*========================================================*/
   /* The current implementation of the defmodules shouldn't */
   /* allow a construct to be defined which would cause an   */
   /* ambiguous reference, but we'll check for it anyway.    */
   /*========================================================*/

   if (count > 1)
     {
      AmbiguousReferenceErrorMessage(theEnv,"defglobal",ValueToString(theValue));
      vPtr->type = SYMBOL;
      vPtr->value = EnvFalseSymbol(theEnv);
      EnvSetEvaluationError(theEnv,true);
      return false;
     }

   /*=================================*/
   /* Get the value of the defglobal. */
   /*=================================*/

   QGetDefglobalValue(theEnv,theGlobal,vPtr);

   return true;
  }

/***************************************************************/
/* QGetDefglobalValue: Returns the value of a global variable. */
/***************************************************************/
bool QGetDefglobalValue(
  Environment *theEnv,
  Defglobal *theGlobal,
  CLIPSValue *vPtr)
  {
   /*===============================================*/
   /* Transfer values which can be copied directly. */
   /*===============================================*/

   vPtr->type = theGlobal->current.type;
   vPtr->value = theGlobal->current.value;
   vPtr->begin = theGlobal->current.begin;
   vPtr->end = theGlobal->current.end;

   /*===========================================================*/
   /* If the global contains a multifield value, return a copy  */
   /* of the value so that routines which use this value are    */
   /* not affected if the value of the global is later changed. */
   /*===========================================================*/

   if (vPtr->type == MULTIFIELD)
     {
      vPtr->value = EnvCreateMultifield(theEnv,(unsigned long) (vPtr->end + 1));
      GenCopyMemory(struct field,vPtr->end + 1,
                                &((struct multifield *) vPtr->value)->theFields[0],
                                &((struct multifield *) theGlobal->current.value)->theFields[theGlobal->current.begin]);
     }

   return true;
  }

/************************************************************/
/* EnvGetDefglobalValue: Returns the value of the specified */
/*   global variable in the supplied CLIPSValue.            */
/************************************************************/
bool EnvGetDefglobalValue(
  Environment *theEnv,
  const char *variableName,
  CLIPSValue *vPtr)
  {
   Defglobal *theDefglobal;

   if ((theDefglobal = EnvFindDefglobal(theEnv,variableName)) == NULL)
     { return false; }

   QGetDefglobalValue(theEnv,theDefglobal,vPtr);

   return true;
  }

/****************************************************************/
/* EnvSetDefglobalValue: Sets the value of the specified global */
/*   variable to the value stored in the supplied CLIPSValue.   */
/****************************************************************/
bool EnvSetDefglobalValue(
  Environment *theEnv,
  const char *variableName,
  CLIPSValue *vPtr)
  {
   Defglobal *theGlobal;

   if ((theGlobal = QFindDefglobal(theEnv,(SYMBOL_HN *) EnvAddSymbol(theEnv,variableName))) == NULL)
     { return false; }

   QSetDefglobalValue(theEnv,theGlobal,vPtr,false);

   return true;
  }

/**********************************************************/
/* DecrementDefglobalBusyCount: Decrements the busy count */
/*   of a defglobal data structure.                       */
/**********************************************************/
static void DecrementDefglobalBusyCount(
  Environment *theEnv,
  Defglobal *theGlobal)
  {
   if (! ConstructData(theEnv)->ClearInProgress) theGlobal->busyCount--;
  }

/**********************************************************/
/* IncrementDefglobalBusyCount: Increments the busy count */
/*   of a defglobal data structure.                       */
/**********************************************************/
static void IncrementDefglobalBusyCount(
  Environment *theEnv,
  Defglobal *theGlobal)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif

   theGlobal->busyCount++;
  }

/***********************************************************************/
/* UpdateDefglobalScope: Updates the scope flag of all the defglobals. */
/***********************************************************************/
void UpdateDefglobalScope(
  Environment *theEnv)
  {
   Defglobal *theDefglobal;
   int moduleCount;
   Defmodule *theModule;
   struct defmoduleItemHeader *theItem;
   
   /*============================*/
   /* Loop through every module. */
   /*============================*/

   for (theModule = EnvGetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
     {
      /*============================================================*/
      /* Loop through every defglobal in the module being examined. */
      /*============================================================*/

      theItem = (struct defmoduleItemHeader *)
                GetModuleItem(theEnv,theModule,DefglobalData(theEnv)->DefglobalModuleIndex);

      for (theDefglobal = (Defglobal *) theItem->firstItem;
           theDefglobal != NULL ;
           theDefglobal = EnvGetNextDefglobal(theEnv,theDefglobal))
        {
         /*====================================================*/
         /* If the defglobal is visible to the current module, */
         /* then mark it as being in scope, otherwise mark it  */
         /* as being out of scope.                             */
         /*====================================================*/

         if (FindImportedConstruct(theEnv,"defglobal",theModule,
                                   ValueToString(theDefglobal->header.name),
                                   &moduleCount,true,NULL) != NULL)
           { theDefglobal->inScope = true; }
         else
           { theDefglobal->inScope = false; }
        }
     }
  }

/*******************************************************/
/* GetNextDefglobalInScope: Returns the next defglobal */
/*   that is scope of the current module. Works in a   */
/*   similar fashion to GetNextDefglobal, but skips    */
/*   defglobals that are out of scope.                 */
/*******************************************************/
Defglobal *GetNextDefglobalInScope(
  Environment *theEnv,
  Defglobal *theGlobal)
  {
   struct defmoduleItemHeader *theItem;

   /*=======================================*/
   /* If we're beginning the search for the */
   /* first defglobal in scope, then ...    */
   /*=======================================*/

   if (theGlobal == NULL)
     {
      /*==============================================*/
      /* If the current module has been changed since */
      /* the last time the scopes were computed, then */
      /* recompute the scopes.                        */
      /*==============================================*/

      if (DefglobalData(theEnv)->LastModuleIndex != DefmoduleData(theEnv)->ModuleChangeIndex)
        {
         UpdateDefglobalScope(theEnv);
         DefglobalData(theEnv)->LastModuleIndex = DefmoduleData(theEnv)->ModuleChangeIndex;
        }

      /*==========================================*/
      /* Get the first module and first defglobal */
      /* to start the search with.                */
      /*==========================================*/

      DefglobalData(theEnv)->TheDefmodule = EnvGetNextDefmodule(theEnv,NULL);
      theItem = (struct defmoduleItemHeader *)
                GetModuleItem(theEnv,DefglobalData(theEnv)->TheDefmodule,DefglobalData(theEnv)->DefglobalModuleIndex);
      theGlobal = (Defglobal *) theItem->firstItem;
     }

   /*==================================================*/
   /* Otherwise, see if the last defglobal returned by */
   /* this function has a defglobal following it.      */
   /*==================================================*/

   else
     { theGlobal = EnvGetNextDefglobal(theEnv,theGlobal); }

   /*======================================*/
   /* Continue looping through the modules */
   /* until a defglobal in scope is found. */
   /*======================================*/

   while (DefglobalData(theEnv)->TheDefmodule != NULL)
     {
      /*=====================================================*/
      /* Loop through the defglobals in the module currently */
      /* being examined to see if one is in scope.           */
      /*=====================================================*/

      for (;
           theGlobal != NULL;
           theGlobal = EnvGetNextDefglobal(theEnv,theGlobal))
        { if (theGlobal->inScope) return theGlobal; }

      /*================================================*/
      /* If a global in scope couldn't be found in this */
      /* module, then move on to the next module.       */
      /*================================================*/

      DefglobalData(theEnv)->TheDefmodule = EnvGetNextDefmodule(theEnv,DefglobalData(theEnv)->TheDefmodule);
      theItem = (struct defmoduleItemHeader *)
                GetModuleItem(theEnv,DefglobalData(theEnv)->TheDefmodule,DefglobalData(theEnv)->DefglobalModuleIndex);
      theGlobal = (Defglobal *) theItem->firstItem;
     }

   /*====================================*/
   /* All the globals in scope have been */
   /* traversed and there are none left. */
   /*====================================*/

   return NULL;
  }

/*##################################*/
/* Additional Environment Functions */
/*##################################*/

const char *EnvDefglobalModule(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
   return GetConstructModuleName((struct constructHeader *) theDefglobal);
  }

const char *EnvGetDefglobalName(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
   return GetConstructNameString((struct constructHeader *) theDefglobal);
  }

const char *EnvGetDefglobalPPForm(
  Environment *theEnv,
  Defglobal *theDefglobal)
  {
   return GetConstructPPForm(theEnv,(struct constructHeader *) theDefglobal);
  }

#endif /* DEFGLOBAL_CONSTRUCT */


