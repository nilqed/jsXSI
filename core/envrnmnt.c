   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/06/16             */
   /*                                                     */
   /*                ENVIRONMENT MODULE                   */
   /*******************************************************/

/*************************************************************/
/* Purpose: Routines for supporting multiple environments.   */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Added code to CreateEnvironment to free        */
/*            already allocated data if one of the malloc    */
/*            calls fail.                                    */
/*                                                           */
/*            Modified AllocateEnvironmentData to print a    */
/*            message if it was unable to allocate memory.   */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added CreateRuntimeEnvironment function.       */
/*                                                           */
/*            Added support for context information when an  */
/*            environment is created (i.e a pointer from the */
/*            CLIPS environment to its parent environment).  */
/*                                                           */
/*      6.30: Added support for passing context information  */ 
/*            to user defined functions and callback         */
/*            functions.                                     */
/*                                                           */
/*            Support for hashing EXTERNAL_ADDRESS data      */
/*            type.                                          */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Removed deallocating message parameter from    */
/*            EnvReleaseMem.                                 */
/*                                                           */
/*            Removed support for BLOCK_MEMORY.              */
/*                                                           */
/*      6.40: Refactored code to reduce header dependencies  */
/*            in sysdep.c.                                   */
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
/*************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "bmathfun.h"
#include "commline.h"
#include "emathfun.h"
#include "engine.h"
#include "filecom.h"
#include "iofun.h"
#include "memalloc.h"
#include "miscfun.h"
#include "multifun.h"
#include "parsefun.h"
#include "prccode.h"
#include "prcdrfun.h"
#include "prdctfun.h"
#include "prntutil.h"
#include "proflfun.h"
#include "router.h"
#include "sortfun.h"
#include "strngfun.h"
#include "sysdep.h"
#include "textpro.h"
#include "utility.h"
#include "watch.h"

#if DEFFACTS_CONSTRUCT
#include "dffctdef.h"
#endif

#if DEFRULE_CONSTRUCT
#include "ruledef.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrccom.h"
#endif

#if DEFFUNCTION_CONSTRUCT
#include "dffnxfun.h"
#endif

#if DEFGLOBAL_CONSTRUCT
#include "globldef.h"
#endif

#if DEFTEMPLATE_CONSTRUCT
#include "tmpltdef.h"
#endif

#if OBJECT_SYSTEM
#include "classini.h"
#endif

#if DEVELOPER
#include "developr.h"
#endif

#include "envrnmnt.h"

#define SIZE_ENVIRONMENT_HASH  131

/****************************************/
/* GLOBAL EXTERNAL FUNCTION DEFINITIONS */
/****************************************/

   extern void                    UserFunctions(void);
   extern void                    EnvUserFunctions(Environment *);

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    RemoveEnvironmentCleanupFunctions(struct environmentData *);
   static void                   *CreateEnvironmentDriver(struct symbolHashNode **,struct floatHashNode **,
                                                          struct integerHashNode **,struct bitMapHashNode **,
                                                          struct externalAddressHashNode **);
   static void                    SystemFunctionDefinitions(Environment *);
   static void                    InitializeKeywords(Environment *);
   static void                    EnvInitializeEnvironment(Environment *,struct symbolHashNode **,struct floatHashNode **,
					   								       struct integerHashNode **,struct bitMapHashNode **,
														   struct externalAddressHashNode **);

/*******************************************************/
/* AllocateEnvironmentData: Allocates environment data */
/*    for the specified environment data record.       */
/*******************************************************/
bool AllocateEnvironmentData(
  Environment *theEnvironment,
  unsigned int position,
  unsigned long size,
  void (*cleanupFunction)(Environment *))
  {
   /*===========================================*/
   /* Environment data can't be of length zero. */
   /*===========================================*/
   
   if (size <= 0)
     {
      printf("\n[ENVRNMNT1] Environment data position %d allocated with size of 0 or less.\n",position);      
      return false;
     }
     
   /*================================================================*/
   /* Check to see if the data position exceeds the maximum allowed. */
   /*================================================================*/
   
   if (position >= MAXIMUM_ENVIRONMENT_POSITIONS)
     {
      printf("\n[ENVRNMNT2] Environment data position %d exceeds the maximum allowed.\n",position);      
      return false;
     }
     
   /*============================================================*/
   /* Check if the environment data has already been registered. */
   /*============================================================*/
   
   if (theEnvironment->theData[position] != NULL)
     {
      printf("\n[ENVRNMNT3] Environment data position %d already allocated.\n",position);      
      return false;
     }
     
   /*====================*/
   /* Allocate the data. */
   /*====================*/
   
   theEnvironment->theData[position] = malloc(size);
   if (theEnvironment->theData[position] == NULL)
     {
      printf("\n[ENVRNMNT4] Environment data position %d could not be allocated.\n",position);      
      return false;
     }
   
   memset(theEnvironment->theData[position],0,size);
   
   /*=============================*/
   /* Store the cleanup function. */
   /*=============================*/
   
   theEnvironment->cleanupFunctions[position] = cleanupFunction;
   
   /*===============================*/
   /* Data successfully registered. */
   /*===============================*/
   
   return true;
  }

/************************************************************/
/* CreateEnvironment: Creates an environment data structure */
/*   and initializes its content to zero/null.              */
/************************************************************/
void *CreateEnvironment()
  {
   return CreateEnvironmentDriver(NULL,NULL,NULL,NULL,NULL);
  }

/**********************************************************/
/* CreateRuntimeEnvironment: Creates an environment data  */
/*   structure and initializes its content to zero/null.  */
/**********************************************************/
void *CreateRuntimeEnvironment(
  struct symbolHashNode **symbolTable,
  struct floatHashNode **floatTable,
  struct integerHashNode **integerTable,
  struct bitMapHashNode **bitmapTable)
  {
   return CreateEnvironmentDriver(symbolTable,floatTable,integerTable,bitmapTable,NULL);
  }
  
/*********************************************************/
/* CreateEnvironmentDriver: Creates an environment data  */
/*   structure and initializes its content to zero/null. */
/*********************************************************/
void *CreateEnvironmentDriver(
  struct symbolHashNode **symbolTable,
  struct floatHashNode **floatTable,
  struct integerHashNode **integerTable,
  struct bitMapHashNode **bitmapTable,
  struct externalAddressHashNode **externalAddressTable)
  {
   struct environmentData *theEnvironment;
   void *theData;
   
   theEnvironment = (struct environmentData *) malloc(sizeof(struct environmentData));
  
   if (theEnvironment == NULL)
     {
      printf("\n[ENVRNMNT5] Unable to create new environment.\n");
      return NULL;
     }

   theData = malloc(sizeof(void *) * MAXIMUM_ENVIRONMENT_POSITIONS);
   
   if (theData == NULL)
     {
      free(theEnvironment);
      printf("\n[ENVRNMNT6] Unable to create environment data.\n");
      return NULL;
     }

   memset(theData,0,sizeof(void *) * MAXIMUM_ENVIRONMENT_POSITIONS);

   theEnvironment->initialized = false;
   theEnvironment->theData = (void **) theData;
   theEnvironment->next = NULL;
   theEnvironment->listOfCleanupEnvironmentFunctions = NULL;
   theEnvironment->context = NULL;
   theEnvironment->routerContext = NULL;
   theEnvironment->functionContext = NULL;
   theEnvironment->callbackContext = NULL;

   /*=============================================*/
   /* Allocate storage for the cleanup functions. */
   /*=============================================*/

   theData = malloc(sizeof(void (*)(struct environmentData *)) * MAXIMUM_ENVIRONMENT_POSITIONS);
   
   if (theData == NULL)
     {
      free(theEnvironment->theData);
      free(theEnvironment);
      printf("\n[ENVRNMNT7] Unable to create environment data.\n");
      return NULL;
     }

   memset(theData,0,sizeof(void (*)(struct environmentData *)) * MAXIMUM_ENVIRONMENT_POSITIONS);
   theEnvironment->cleanupFunctions = (void (**)(Environment *))theData;

   EnvInitializeEnvironment(theEnvironment,symbolTable,floatTable,integerTable,bitmapTable,externalAddressTable);

   return(theEnvironment);
  }

/**********************************************/
/* GetEnvironmentContext: Returns the context */
/*   of the specified environment.            */
/**********************************************/
void *GetEnvironmentContext(
  Environment *theEnvironment)
  {
   return(((struct environmentData *) theEnvironment)->context);
  } 

/*******************************************/
/* SetEnvironmentContext: Sets the context */
/*   of the specified environment.         */
/*******************************************/
void *SetEnvironmentContext(
  Environment *theEnvironment,
  void *theContext)
  {
   void *oldContext;
   
   oldContext = ((struct environmentData *) theEnvironment)->context;
  
   ((struct environmentData *) theEnvironment)->context = theContext;
   
   return oldContext;
  } 

/***************************************************/
/* GetEnvironmentRouterContext: Returns the router */
/*   context of the specified environment.         */
/***************************************************/
void *GetEnvironmentRouterContext(
  Environment *theEnvironment)
  {
   return(((struct environmentData *) theEnvironment)->routerContext);
  } 

/************************************************/
/* SetEnvironmentRouterContext: Sets the router */
/*   context of the specified environment.      */
/************************************************/
void *SetEnvironmentRouterContext(
  Environment *theEnvironment,
  void *theRouterContext)
  {
   void *oldRouterContext;
   
   oldRouterContext = ((struct environmentData *) theEnvironment)->routerContext;
  
   ((struct environmentData *) theEnvironment)->routerContext = theRouterContext;
   
   return oldRouterContext;
  } 

/*******************************************************/
/* GetEnvironmentFunctionContext: Returns the function */
/*   context of the specified environment.             */
/*******************************************************/
void *GetEnvironmentFunctionContext(
  Environment *theEnvironment)
  {
   return(((struct environmentData *) theEnvironment)->functionContext);
  } 

/**************************************************/
/* SetEnvironmentFunctionContext: Sets the router */
/*   context of the specified environment.        */
/**************************************************/
void *SetEnvironmentFunctionContext(
  Environment *theEnvironment,
  void *theFunctionContext)
  {
   void *oldFunctionContext;
   
   oldFunctionContext = ((struct environmentData *) theEnvironment)->functionContext;
  
   ((struct environmentData *) theEnvironment)->functionContext = theFunctionContext;
   
   return oldFunctionContext;
  } 

/*******************************************************/
/* GetEnvironmentCallbackContext: Returns the callback */
/*   context of the specified environment.             */
/*******************************************************/
void *GetEnvironmentCallbackContext(
  Environment *theEnvironment)
  {
   return(((struct environmentData *) theEnvironment)->callbackContext);
  } 

/****************************************************/
/* SetEnvironmentCallbackContext: Sets the callback */
/*   context of the specified environment.          */
/****************************************************/
void *SetEnvironmentCallbackContext(
  Environment *theEnvironment,
  void *theCallbackContext)
  {
   void *oldCallbackContext;
   
   oldCallbackContext = ((struct environmentData *) theEnvironment)->callbackContext;
  
   ((struct environmentData *) theEnvironment)->callbackContext = theCallbackContext;
   
   return oldCallbackContext;
  } 
  
/**********************************************/
/* DestroyEnvironment: Destroys the specified */
/*   environment returning all of its memory. */
/**********************************************/
bool DestroyEnvironment(
  Environment *theEnvironment)
  {   
   struct environmentCleanupFunction *cleanupPtr;
   int i;
   struct memoryData *theMemData;
   bool rv = true;
   /*
   if (EvaluationData(theEnvironment)->CurrentExpression != NULL)
     { return false; }
     
#if DEFRULE_CONSTRUCT
   if (EngineData(theEnvironment)->ExecutingRule != NULL)
     { return false; }
#endif
*/
   theMemData = MemoryData(theEnvironment);

   EnvReleaseMem(theEnvironment,-1);

   for (i = 0; i < MAXIMUM_ENVIRONMENT_POSITIONS; i++)
     {
      if (theEnvironment->cleanupFunctions[i] != NULL)
        { (*theEnvironment->cleanupFunctions[i])(theEnvironment); }
     }
     
   free(theEnvironment->cleanupFunctions);
     
   for (cleanupPtr = theEnvironment->listOfCleanupEnvironmentFunctions;
        cleanupPtr != NULL;
        cleanupPtr = cleanupPtr->next)
     { (*cleanupPtr->func)(theEnvironment); }

   RemoveEnvironmentCleanupFunctions(theEnvironment);
   
   EnvReleaseMem(theEnvironment,-1);
     
   if ((theMemData->MemoryAmount != 0) || (theMemData->MemoryCalls != 0))
     {
      printf("\n[ENVRNMNT8] Environment data not fully deallocated.\n"); 
      printf("\n[ENVRNMNT8] MemoryAmount = %ld.\n",(long) theMemData->MemoryAmount); 
      printf("\n[ENVRNMNT8] MemoryCalls = %ld.\n",(long) theMemData->MemoryCalls); 
      rv = false;
     }

#if (MEM_TABLE_SIZE > 0)
   free(theMemData->MemoryTable);
#endif

   for (i = 0; i < MAXIMUM_ENVIRONMENT_POSITIONS; i++)
     {
      if (theEnvironment->theData[i] != NULL)
        {
         free(theEnvironment->theData[i]);
         theEnvironment->theData[i] = NULL;
        }
     }
     
   free(theEnvironment->theData);
   
   free(theEnvironment);
   
   return(rv);
  } 
 
/**************************************************/
/* AddEnvironmentCleanupFunction: Adds a function */
/*   to the ListOfCleanupEnvironmentFunctions.    */
/**************************************************/
bool AddEnvironmentCleanupFunction(
  Environment *theEnv,
  const char *name,
  void (*functionPtr)(Environment *),
  int priority)
  {
   struct environmentCleanupFunction *newPtr, *currentPtr, *lastPtr = NULL;
     
   newPtr = (struct environmentCleanupFunction *) malloc(sizeof(struct environmentCleanupFunction));
   if (newPtr == NULL)
     { return false; }

   newPtr->name = name;
   newPtr->func = functionPtr;
   newPtr->priority = priority;

   if (theEnv->listOfCleanupEnvironmentFunctions == NULL)
     {
      newPtr->next = NULL;
      theEnv->listOfCleanupEnvironmentFunctions = newPtr;
      return true;
     }

   currentPtr = theEnv->listOfCleanupEnvironmentFunctions;
   while ((currentPtr != NULL) ? (priority < currentPtr->priority) : false)
     {
      lastPtr = currentPtr;
      currentPtr = currentPtr->next;
     }

   if (lastPtr == NULL)
     {
      newPtr->next = theEnv->listOfCleanupEnvironmentFunctions;
      theEnv->listOfCleanupEnvironmentFunctions = newPtr;
     }
   else
     {
      newPtr->next = currentPtr;
      lastPtr->next = newPtr;
     }
     
   return true;
  }

/**************************************************/
/* RemoveEnvironmentCleanupFunctions: Removes the */
/*   list of environment cleanup functions.       */
/**************************************************/
static void RemoveEnvironmentCleanupFunctions(
  struct environmentData *theEnv)
  {   
   struct environmentCleanupFunction *nextPtr;
      
   while (theEnv->listOfCleanupEnvironmentFunctions != NULL)
     { 
      nextPtr = theEnv->listOfCleanupEnvironmentFunctions->next;
      free(theEnv->listOfCleanupEnvironmentFunctions);
      theEnv->listOfCleanupEnvironmentFunctions = nextPtr;
     }
  }

/*****************************************************/
/* EnvInitializeEnvironment: Performs initialization */
/*   of the KB environment.                          */
/*****************************************************/
static void EnvInitializeEnvironment(
  Environment *theEnvironment,
  struct symbolHashNode **symbolTable,
  struct floatHashNode **floatTable,
  struct integerHashNode **integerTable,
  struct bitMapHashNode **bitmapTable,
  struct externalAddressHashNode **externalAddressTable)
  {   
   /*================================================*/
   /* Don't allow the initialization to occur twice. */
   /*================================================*/

   if (theEnvironment->initialized) return;
     
   /*================================*/
   /* Initialize the memory manager. */
   /*================================*/

   InitializeMemory(theEnvironment);

   /*===================================================*/
   /* Initialize environment data for various features. */
   /*===================================================*/
   
   InitializeCommandLineData(theEnvironment);
#if CONSTRUCT_COMPILER && (! RUN_TIME)
   InitializeConstructCompilerData(theEnvironment);
#endif
   InitializeConstructData(theEnvironment);
   InitializeEvaluationData(theEnvironment);
   InitializeExternalFunctionData(theEnvironment);
   InitializePrettyPrintData(theEnvironment);
   InitializePrintUtilityData(theEnvironment);
   InitializeScannerData(theEnvironment);
   InitializeSystemDependentData(theEnvironment);
   InitializeUserDataData(theEnvironment);
   InitializeUtilityData(theEnvironment);
#if DEBUGGING_FUNCTIONS
   InitializeWatchData(theEnvironment);
#endif
   
   /*===============================================*/
   /* Initialize the hash tables for atomic values. */
   /*===============================================*/

   InitializeAtomTables(theEnvironment,symbolTable,floatTable,integerTable,bitmapTable,externalAddressTable);

   /*=========================================*/
   /* Initialize file and string I/O routers. */
   /*=========================================*/

   InitializeDefaultRouters(theEnvironment);

   /*=========================================================*/
   /* Initialize some system dependent features such as time. */
   /*=========================================================*/

   InitializeNonportableFeatures(theEnvironment);

   /*=============================================*/
   /* Register system and user defined functions. */
   /*=============================================*/

   SystemFunctionDefinitions(theEnvironment);
   UserFunctions();
   EnvUserFunctions(theEnvironment);

   /*====================================*/
   /* Initialize the constraint manager. */
   /*====================================*/

   InitializeConstraints(theEnvironment);

   /*==========================================*/
   /* Initialize the expression hash table and */
   /* pointers to specific functions.          */
   /*==========================================*/

   InitExpressionData(theEnvironment);

   /*===================================*/
   /* Initialize the construct manager. */
   /*===================================*/

#if ! RUN_TIME
   InitializeConstructs(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defmodule construct. */
   /*=====================================*/

   AllocateDefmoduleGlobals(theEnvironment);

   /*===================================*/
   /* Initialize the defrule construct. */
   /*===================================*/

#if DEFRULE_CONSTRUCT
   InitializeDefrules(theEnvironment);
#endif

   /*====================================*/
   /* Initialize the deffacts construct. */
   /*====================================*/

#if DEFFACTS_CONSTRUCT
   InitializeDeffacts(theEnvironment);
#endif

   /*=====================================================*/
   /* Initialize the defgeneric and defmethod constructs. */
   /*=====================================================*/

#if DEFGENERIC_CONSTRUCT
   SetupGenericFunctions(theEnvironment);
#endif

   /*=======================================*/
   /* Initialize the deffunction construct. */
   /*=======================================*/

#if DEFFUNCTION_CONSTRUCT
   SetupDeffunctions(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defglobal construct. */
   /*=====================================*/

#if DEFGLOBAL_CONSTRUCT
   InitializeDefglobals(theEnvironment);
#endif

   /*=======================================*/
   /* Initialize the deftemplate construct. */
   /*=======================================*/

#if DEFTEMPLATE_CONSTRUCT
   InitializeDeftemplates(theEnvironment);
#endif

   /*=============================*/
   /* Initialize COOL constructs. */
   /*=============================*/

#if OBJECT_SYSTEM
   SetupObjectSystem(theEnvironment);
#endif

   /*=====================================*/
   /* Initialize the defmodule construct. */
   /*=====================================*/

   InitializeDefmodules(theEnvironment);

   /*======================================================*/
   /* Register commands and functions for development use. */
   /*======================================================*/

#if DEVELOPER
   DeveloperCommands(theEnvironment);
#endif

   /*=========================================*/
   /* Install the special function primitives */
   /* used by procedural code in constructs.  */
   /*=========================================*/

   InstallProcedurePrimitives(theEnvironment);

   /*==============================================*/
   /* Install keywords in the symbol table so that */
   /* they are available for command completion.   */
   /*==============================================*/

   InitializeKeywords(theEnvironment);

   /*========================*/
   /* Issue a clear command. */
   /*========================*/
   
   EnvClear(theEnvironment);

   /*=============================*/
   /* Initialization is complete. */
   /*=============================*/

   theEnvironment->initialized = true;
  }

/**************************************************/
/* SystemFunctionDefinitions: Sets up definitions */
/*   of system defined functions.                 */
/**************************************************/
static void SystemFunctionDefinitions(
  Environment *theEnv)
  {
   ProceduralFunctionDefinitions(theEnv);
   MiscFunctionDefinitions(theEnv);

#if IO_FUNCTIONS
   IOFunctionDefinitions(theEnv);
#endif

   PredicateFunctionDefinitions(theEnv);
   BasicMathFunctionDefinitions(theEnv);
   FileCommandDefinitions(theEnv);
   SortFunctionDefinitions(theEnv);

#if DEBUGGING_FUNCTIONS
   WatchFunctionDefinitions(theEnv);
#endif

#if MULTIFIELD_FUNCTIONS
   MultifieldFunctionDefinitions(theEnv);
#endif

#if STRING_FUNCTIONS
   StringFunctionDefinitions(theEnv);
#endif

#if EXTENDED_MATH_FUNCTIONS
   ExtendedMathFunctionDefinitions(theEnv);
#endif

#if TEXTPRO_FUNCTIONS
   HelpFunctionDefinitions(theEnv);
#endif

#if CONSTRUCT_COMPILER && (! RUN_TIME)
   ConstructsToCCommandDefinition(theEnv);
#endif

#if PROFILING_FUNCTIONS
   ConstructProfilingFunctionDefinitions(theEnv);
#endif

   ParseFunctionDefinitions(theEnv);
  }

/*********************************************/
/* InitializeKeywords: Adds key words to the */
/*   symbol table so that they are available */
/*   for command completion.                 */
/*********************************************/
static void InitializeKeywords(
  Environment *theEnv)
  {
#if (! RUN_TIME) && WINDOW_INTERFACE
   void *ts;

   /*====================*/
   /* construct keywords */
   /*====================*/

   ts = EnvAddSymbol(theEnv,"defrule");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"defglobal");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"deftemplate");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"deffacts");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"deffunction");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"defmethod");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"defgeneric");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"defclass");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"defmessage-handler");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"definstances");
   IncrementSymbolCount(ts);

   /*=======================*/
   /* set-strategy keywords */
   /*=======================*/

   ts = EnvAddSymbol(theEnv,"depth");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"breadth");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"lex");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"mea");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"simplicity");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"complexity");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"random");
   IncrementSymbolCount(ts);

   /*==================================*/
   /* set-salience-evaluation keywords */
   /*==================================*/

   ts = EnvAddSymbol(theEnv,"when-defined");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"when-activated");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"every-cycle");
   IncrementSymbolCount(ts);

   /*======================*/
   /* deftemplate keywords */
   /*======================*/

   ts = EnvAddSymbol(theEnv,"field");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"multifield");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"default");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"type");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-symbols");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-strings");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-numbers");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-integers");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-floats");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-values");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"min-number-of-elements");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"max-number-of-elements");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"NONE");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"VARIABLE");
   IncrementSymbolCount(ts);

   /*==================*/
   /* defrule keywords */
   /*==================*/

   ts = EnvAddSymbol(theEnv,"declare");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"salience");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"test");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"or");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"and");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"not");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"logical");
   IncrementSymbolCount(ts);

   /*===============*/
   /* COOL keywords */
   /*===============*/

   ts = EnvAddSymbol(theEnv,"is-a");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"role");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"abstract");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"concrete");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"pattern-match");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"reactive");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"non-reactive");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"slot");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"field");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"multiple");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"single");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"storage");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"shared");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"local");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"access");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"read");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"write");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"read-only");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"read-write");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"initialize-only");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"propagation");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"inherit");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"no-inherit");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"source");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"composite");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"exclusive");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-lexemes");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"allowed-instances");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"around");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"before");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"primary");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"after");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"of");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"self");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"visibility");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"override-message");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"private");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"public");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"create-accessor");
   IncrementSymbolCount(ts);

   /*================*/
   /* watch keywords */
   /*================*/

   ts = EnvAddSymbol(theEnv,"compilations");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"deffunctions");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"globals");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"rules");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"activations");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"statistics");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"facts");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"generic-functions");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"methods");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"instances");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"slots");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"messages");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"message-handlers");
   IncrementSymbolCount(ts);
   ts = EnvAddSymbol(theEnv,"focus");
   IncrementSymbolCount(ts);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

