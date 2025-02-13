   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*                    BLOAD MODULE                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides core routines for loading constructs    */
/*   from a binary file.                                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
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
/*            Callbacks must be environment aware.           */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include "argacces.h"
#include "bsave.h"
#include "constrct.h"
#include "cstrnbin.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "router.h"
#include "utility.h"

#include "bload.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct FunctionDefinition **ReadNeededFunctions(Environment *,long *,bool *);
   static struct FunctionDefinition  *FastFindFunction(Environment *,const char *,struct FunctionDefinition *);
   static bool                        ClearBload(Environment *);
   static void                        ClearBloadCallback(Environment *);
   static void                        AbortBload(Environment *);
   static bool                        BloadOutOfMemoryFunction(Environment *,size_t);
   static void                        DeallocateBloadData(Environment *);

/**********************************************/
/* InitializeBloadData: Allocates environment */
/*    data for the bload command.             */
/**********************************************/
void InitializeBloadData(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,BLOAD_DATA,sizeof(struct bloadData),NULL);
   AddEnvironmentCleanupFunction(theEnv,"bload",DeallocateBloadData,-1500);
   EnvAddClearFunction(theEnv,"bload",ClearBloadCallback,10000);

   BloadData(theEnv)->BinaryPrefixID = "\1\2\3\4CLIPS";
   BloadData(theEnv)->BinaryVersionID = "V6.30";
  }
  
/************************************************/
/* DeallocateBloadData: Deallocates environment */
/*    data for the bload command.               */
/************************************************/
static void DeallocateBloadData(
  Environment *theEnv)
  {   
   DeallocateCallList(theEnv,BloadData(theEnv)->BeforeBloadFunctions);
   DeallocateCallList(theEnv,BloadData(theEnv)->AfterBloadFunctions);
   DeallocateCallList(theEnv,BloadData(theEnv)->ClearBloadReadyFunctions);
   DeallocateCallList(theEnv,BloadData(theEnv)->AbortBloadFunctions);
  }

/******************************/
/* EnvBload: C access routine */
/*   for the bload command.   */
/******************************/
bool EnvBload(
  Environment *theEnv,
  const char *fileName)
  {
   long numberOfFunctions;
   unsigned long space;
   bool error;
   char IDbuffer[20];   
   char constructBuffer[CONSTRUCT_HEADER_SIZE];
   struct BinaryItem *biPtr;
   struct callFunctionItem *bfPtr;

   /*================*/
   /* Open the file. */
   /*================*/

   if (GenOpenReadBinary(theEnv,"bload",fileName) == 0)
     {
      OpenErrorMessage(theEnv,"bload",fileName);
      return false;
     }

   /*=====================================*/
   /* Determine if this is a binary file. */
   /*=====================================*/

   GenReadBinary(theEnv,IDbuffer,(unsigned long) strlen(BloadData(theEnv)->BinaryPrefixID) + 1);
   if (strcmp(IDbuffer,BloadData(theEnv)->BinaryPrefixID) != 0)
     {
      PrintErrorID(theEnv,"BLOAD",2,false);
      EnvPrintRouter(theEnv,WERROR,"File ");
      EnvPrintRouter(theEnv,WERROR,fileName);
      EnvPrintRouter(theEnv,WERROR," is not a binary construct file.\n");
      GenCloseBinary(theEnv);
      return false;
     }

   /*=======================================*/
   /* Determine if it's a binary file using */
   /* a format from a different version.    */
   /*=======================================*/

   GenReadBinary(theEnv,IDbuffer,(unsigned long) strlen(BloadData(theEnv)->BinaryVersionID) + 1);
   if (strcmp(IDbuffer,BloadData(theEnv)->BinaryVersionID) != 0)
     {
      PrintErrorID(theEnv,"BLOAD",3,false);
      EnvPrintRouter(theEnv,WERROR,"File ");
      EnvPrintRouter(theEnv,WERROR,fileName);
      EnvPrintRouter(theEnv,WERROR," is an incompatible binary construct file.\n");
      GenCloseBinary(theEnv);
      return false;
     }
     
   /*====================*/
   /* Clear environment. */
   /*====================*/

   if (BloadData(theEnv)->BloadActive)
     {
      if (ClearBload(theEnv) == false)
        {
         GenCloseBinary(theEnv);
         return false;
        }
     }

   /*=================================*/
   /* Determine if the KB environment */
   /* was successfully cleared.       */
   /*=================================*/

   if (ClearReady(theEnv) == false)
     {
      GenCloseBinary(theEnv);
      EnvPrintRouter(theEnv,WERROR,"The ");
      EnvPrintRouter(theEnv,WERROR,APPLICATION_NAME);
      EnvPrintRouter(theEnv,WERROR," environment could not be cleared.\n");
      EnvPrintRouter(theEnv,WERROR,"Binary load cannot continue.\n");
      return false;
     }

   /*==================================*/
   /* Call the list of functions to be */
   /* executed before a bload occurs.  */
   /*==================================*/

   ConstructData(theEnv)->ClearInProgress = true;
   for (bfPtr = BloadData(theEnv)->BeforeBloadFunctions;
        bfPtr != NULL;
        bfPtr = bfPtr->next)
     { (*bfPtr->func)(theEnv); }

   ConstructData(theEnv)->ClearInProgress = false;

   /*====================================================*/
   /* Read in the functions needed by this binary image. */
   /*====================================================*/

   BloadData(theEnv)->FunctionArray = ReadNeededFunctions(theEnv,&numberOfFunctions,&error);
   if (error)
     {
      GenCloseBinary(theEnv);
      AbortBload(theEnv);
      return false;
     }

   /*================================================*/
   /* Read in the atoms needed by this binary image. */
   /*================================================*/

   ReadNeededAtomicValues(theEnv);

   /*===========================================*/
   /* Determine the number of expressions to be */
   /* read and allocate the appropriate space   */
   /*===========================================*/

   AllocateExpressions(theEnv);

   /*==========================================================*/
   /* Read in the memory requirements of the constructs stored */
   /* in this binary image and allocate the necessary space    */
   /*==========================================================*/

   for (GenReadBinary(theEnv,constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE);
        strncmp(constructBuffer,BloadData(theEnv)->BinaryPrefixID,CONSTRUCT_HEADER_SIZE) != 0;
        GenReadBinary(theEnv,constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE))
     {
      bool found;

      /*================================================*/
      /* Search for the construct type in the list of   */
      /* binary items. If found, allocate the storage   */
      /* needed by the construct for this binary image. */
      /*================================================*/

      found = false;
      for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
           biPtr != NULL;
           biPtr = biPtr->next)
        {
         if (strncmp(biPtr->name,constructBuffer,CONSTRUCT_HEADER_SIZE) == 0)
           {
            if (biPtr->bloadStorageFunction != NULL)
              {
               (*biPtr->bloadStorageFunction)(theEnv);
               found = true;
              }
            break;
           }
        }

      /*==========================================*/
      /* If the construct type wasn't found, skip */
      /* the storage binary load information for  */
      /* this construct.                          */
      /*==========================================*/

      if (! found)
        {
         GenReadBinary(theEnv,&space,(unsigned long) sizeof(unsigned long));
         GetSeekCurBinary(theEnv,(long) space);
         if (space != 0)
           {
            EnvPrintRouter(theEnv,WDIALOG,"\nSkipping ");
            EnvPrintRouter(theEnv,WDIALOG,constructBuffer);
            EnvPrintRouter(theEnv,WDIALOG," constructs because of unavailability\n");
           }
        }
     }

   /*======================================*/
   /* Refresh the pointers in expressions. */
   /*======================================*/

   RefreshExpressions(theEnv);

   /*==========================*/
   /* Read in the constraints. */
   /*==========================*/

   ReadNeededConstraints(theEnv);

   /*======================================================*/
   /* Read in the constructs stored in this binary image.  */
   /*======================================================*/

   for (GenReadBinary(theEnv,constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE);
        strncmp(constructBuffer,BloadData(theEnv)->BinaryPrefixID,CONSTRUCT_HEADER_SIZE) != 0;
        GenReadBinary(theEnv,constructBuffer,(unsigned long) CONSTRUCT_HEADER_SIZE))
     {
      bool found;

      /*==================================================*/
      /* Search for the function to load the construct    */
      /* into the previously allocated storage. If found, */
      /* call the function to load the construct.         */
      /*==================================================*/

      found = false;
      for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
           biPtr != NULL;
           biPtr = biPtr->next)
        {
         if (strncmp(biPtr->name,constructBuffer,CONSTRUCT_HEADER_SIZE) == 0)
           {
            if (biPtr->bloadFunction != NULL)
              {
               (*biPtr->bloadFunction)(theEnv);
               found = true;
              }
            break;
           }
        }

      /*==========================================*/
      /* If the construct type wasn't found, skip */
      /* the binary data for this construct.      */
      /*==========================================*/

      if (! found)
        {
         GenReadBinary(theEnv,&space,(unsigned long) sizeof(unsigned long));
         GetSeekCurBinary(theEnv,(long) space);
        }
     }

   /*=================*/
   /* Close the file. */
   /*=================*/

   GenCloseBinary(theEnv);

   /*========================================*/
   /* Free up temporary storage used for the */
   /* function and atomic value information. */
   /*========================================*/

   if (BloadData(theEnv)->FunctionArray != NULL)
     {
      genfree(theEnv,BloadData(theEnv)->FunctionArray,
              sizeof(struct FunctionDefinition *) * numberOfFunctions);
     }
   FreeAtomicValueStorage(theEnv);

   /*==================================*/
   /* Call the list of functions to be */
   /* executed after a bload occurs.   */
   /*==================================*/

   for (bfPtr = BloadData(theEnv)->AfterBloadFunctions;
        bfPtr != NULL;
        bfPtr = bfPtr->next)
     { (*bfPtr->func)(theEnv); }

   /*=======================================*/
   /* Add a clear function to remove binary */
   /* load when a clear command is issued.  */
   /*=======================================*/

   BloadData(theEnv)->BloadActive = true;

   /*=============================*/
   /* Return true to indicate the */
   /* binary load was successful. */
   /*=============================*/

   return true;
  }

/************************************************************
  NAME         : BloadandRefresh
  DESCRIPTION  : Loads and refreshes objects - will bload
                 all objects at once, if possible, but
                 will aslo work in increments if memory is
                 restricted
  INPUTS       : 1) the number of objects to bload and update
                 2) the size of one object
                 3) An update function which takes a bloaded
                    object buffer and the index of the object
                    to refresh as arguments
  RETURNS      : Nothing useful
  SIDE EFFECTS : Objects bloaded and updated
  NOTES        : Assumes binary file pointer is positioned
                 for bloads of the objects
 ************************************************************/
void BloadandRefresh(
  Environment *theEnv,
  long objcnt,
  size_t objsz,
  void (*objupdate)(Environment *,void *,long))
  {
   long i,bi;
   char *buf;
   long objsmaxread,objsread;
   size_t space;
   OutOfMemoryFunction *oldOutOfMemoryFunction;

   if (objcnt == 0L) return;

   oldOutOfMemoryFunction = EnvSetOutOfMemoryFunction(theEnv,BloadOutOfMemoryFunction);
   objsmaxread = objcnt;
   do
     {
      space = objsmaxread * objsz;
      buf = (char *) genalloc(theEnv,space);
      if (buf == NULL)
        {
         if ((objsmaxread / 2) == 0)
           {
            if ((*oldOutOfMemoryFunction)(theEnv,space) == true)
              {
               EnvSetOutOfMemoryFunction(theEnv,oldOutOfMemoryFunction);
               return;
              }
           }
         else
           objsmaxread /= 2;
        }
     }
   while (buf == NULL);

   EnvSetOutOfMemoryFunction(theEnv,oldOutOfMemoryFunction);

   i = 0L;
   do
     {
      objsread = (objsmaxread > (objcnt - i)) ? (objcnt - i) : objsmaxread;
      GenReadBinary(theEnv,buf,objsread * objsz);
      for (bi = 0L ; bi < objsread ; bi++ , i++)
        (*objupdate)(theEnv,buf + objsz * bi,i);
     }
   while (i < objcnt);
   genfree(theEnv,buf,space);
  }

/**********************************************/
/* ReadNeededFunctions: Reads in the names of */
/*   functions needed by the binary image.    */
/**********************************************/
static struct FunctionDefinition **ReadNeededFunctions(
  Environment *theEnv,
  long int *numberOfFunctions,
  bool *error)
  {
   char *functionNames, *namePtr;
   unsigned long int space;
   size_t temp;
   long i;
   struct FunctionDefinition **newFunctionArray, *functionPtr;
   int functionsNotFound = 0;

   /*===================================================*/
   /* Determine the number of function names to be read */
   /* and the space required for them.                  */
   /*===================================================*/

   GenReadBinary(theEnv,numberOfFunctions,(unsigned long) sizeof(long int));
   GenReadBinary(theEnv,&space,(unsigned long) sizeof(unsigned long int));
   if (*numberOfFunctions == 0)
     {
      *error = false;
      return NULL;
     }

   /*=======================================*/
   /* Allocate area for strings to be read. */
   /*=======================================*/

   functionNames = (char *) genalloc(theEnv,space);
   GenReadBinary(theEnv,functionNames,space);

   /*====================================================*/
   /* Store the function pointers in the function array. */
   /*====================================================*/

   temp = (unsigned long) sizeof(struct FunctionDefinition *) * *numberOfFunctions;
   newFunctionArray = (struct FunctionDefinition **) genalloc(theEnv,temp);
   namePtr = functionNames;
   functionPtr = NULL;
   for (i = 0; i < *numberOfFunctions; i++)
     {
      if ((functionPtr = FastFindFunction(theEnv,namePtr,functionPtr)) == NULL)
        {
         if (! functionsNotFound)
           {
            PrintErrorID(theEnv,"BLOAD",6,false);
            EnvPrintRouter(theEnv,WERROR,"The following undefined functions are ");
            EnvPrintRouter(theEnv,WERROR,"referenced by this binary image:\n");
           }

         EnvPrintRouter(theEnv,WERROR,"   ");
         EnvPrintRouter(theEnv,WERROR,namePtr);
         EnvPrintRouter(theEnv,WERROR,"\n");
         functionsNotFound = 1;
        }

      newFunctionArray[i] = functionPtr;
      namePtr += strlen(namePtr) + 1;
     }

   /*==========================================*/
   /* Free the memory used by the name buffer. */
   /*==========================================*/

   genfree(theEnv,functionNames,space);

   /*==================================================*/
   /* If any of the required functions were not found, */
   /* then free the memory used by the function array. */
   /*==================================================*/

   if (functionsNotFound)
     {
      genfree(theEnv,newFunctionArray,temp);
      newFunctionArray = NULL;
     }

   /*===================================*/
   /* Set globals to appropriate values */
   /* and return the function array.    */
   /*===================================*/

   *error = functionsNotFound;
   return(newFunctionArray);
  }

/*****************************************/
/* FastFindFunction: Search the function */
/*   list for a specific function.       */
/*****************************************/
static struct FunctionDefinition *FastFindFunction(
  Environment *theEnv,
  const char *functionName,
  struct FunctionDefinition *lastFunction)
  {
   struct FunctionDefinition *theList, *theFunction;

   /*========================*/
   /* Get the function list. */
   /*========================*/

   theList = GetFunctionList(theEnv);
   if (theList == NULL) { return NULL; }

   /*=======================================*/
   /* If we completed a previous function   */
   /* search, start where we last left off. */
   /*=======================================*/

   if (lastFunction != NULL)
     { theFunction = lastFunction->next; }
   else
     { theFunction = theList; }

   /*======================================================*/
   /* Traverse the rest of the function list searching for */
   /* the named function wrapping around if necessary.     */
   /*======================================================*/

   while (strcmp(functionName,ValueToString(theFunction->callFunctionName)) != 0)
     {
      theFunction = theFunction->next;
      if (theFunction == lastFunction) return NULL;
      if (theFunction == NULL) theFunction = theList;
     }

   /*=======================*/
   /* Return the pointer to */
   /* the found function.   */
   /*=======================*/

   return(theFunction);
  }

/******************************************/
/* Bloaded: Returns true if the current   */
/*   environment is the result of a bload */
/*   command, otherwise returns false.    */
/******************************************/
bool Bloaded(
  Environment *theEnv)
  {
   return BloadData(theEnv)->BloadActive;
  }

/***************************************/
/* ClearBloadCallback: Clears a binary */
/*   image from the KB environment.    */
/***************************************/
static void ClearBloadCallback(
  Environment *theEnv)
  {
   ClearBload(theEnv);
  }

/*************************************/
/* ClearBload: Clears a binary image */
/*   from the KB environment.        */
/*************************************/
static bool ClearBload(
  Environment *theEnv)
  {
   struct BinaryItem *biPtr;
   struct callFunctionItem *bfPtr;
   bool ready, error;

   /*======================================*/
   /* If bload is not active, then there's */
   /* no need to clear bload data.         */
   /*======================================*/
   
   if (! BloadData(theEnv)->BloadActive)
     { return true; }
     
   /*=================================================*/
   /* Make sure it's safe to clear the bloaded image. */
   /*=================================================*/

   error = false;
   for (bfPtr = BloadData(theEnv)->ClearBloadReadyFunctions;
        bfPtr != NULL;
        bfPtr = bfPtr->next)
     {
      ready = (* ((int (*)(void *)) bfPtr->func))(theEnv);

      if (ready == false)
        {
         if (! error)
           {
            PrintErrorID(theEnv,"BLOAD",5,false);
            EnvPrintRouter(theEnv,WERROR,
                       "Some constructs are still in use by the current binary image:\n");
           }
         EnvPrintRouter(theEnv,WERROR,"   ");
         EnvPrintRouter(theEnv,WERROR,bfPtr->name);
         EnvPrintRouter(theEnv,WERROR,"\n");
         error = true;
        }
     }

   /*==================================================*/
   /* If some constructs are still in use and can't be */
   /* cleared, indicate the binary load can't continue */
   /* and return false to indicate this condition.     */
   /*==================================================*/

   if (error == true)
     {
      EnvPrintRouter(theEnv,WERROR,"Binary clear cannot continue.\n");
      return false;
     }

   /*=============================*/
   /* Call bload clear functions. */
   /*=============================*/

   for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     { if (biPtr->clearFunction != NULL) (*biPtr->clearFunction)(theEnv); }

   /*===========================*/
   /* Free bloaded expressions. */
   /*===========================*/

   ClearBloadedExpressions(theEnv);

   /*===========================*/
   /* Free bloaded constraints. */
   /*===========================*/

   ClearBloadedConstraints(theEnv);

   /*==================================*/
   /* Remove the bload clear function. */
   /*==================================*/

   BloadData(theEnv)->BloadActive = false;

   /*====================================*/
   /* Return true to indicate the binary */
   /* image was successfully cleared.    */
   /*====================================*/

   return true;
  }

/*************************************************/
/* AbortBload: Cleans up effects of before-bload */
/*   functions in event of failure.              */
/*************************************************/
static void AbortBload(
  Environment *theEnv)
  {
   struct callFunctionItem *bfPtr;

   for (bfPtr = BloadData(theEnv)->AbortBloadFunctions;
        bfPtr != NULL;
        bfPtr = bfPtr->next)
     { (*bfPtr->func)(theEnv); }
  }

/********************************************/
/* AddBeforeBloadFunction: Adds a function  */
/*   to the list of functions called before */
/*   a binary load occurs.                  */
/********************************************/
void AddBeforeBloadFunction(
  Environment *theEnv,
  const char *name,
  void (*func)(Environment *),
  int priority)
  {
   BloadData(theEnv)->BeforeBloadFunctions =
     AddFunctionToCallList(theEnv,name,priority,func,BloadData(theEnv)->BeforeBloadFunctions);
  }

/*******************************************/
/* AddAfterBloadFunction: Adds a function  */
/*   to the list of functions called after */
/*   a binary load occurs.                 */
/*******************************************/
void AddAfterBloadFunction(
  Environment *theEnv,
  const char *name,
  void (*func)(Environment *),
  int priority)
  {
   BloadData(theEnv)->AfterBloadFunctions =
      AddFunctionToCallList(theEnv,name,priority,func,BloadData(theEnv)->AfterBloadFunctions);
  }

/**************************************************/
/* AddClearBloadReadyFunction: Adds a function to */
/*   the list of functions called to determine if */
/*   a binary image can be cleared.               */
/**************************************************/
void AddClearBloadReadyFunction(
  Environment *theEnv,
  const char *name,
  int (*func)(Environment *), // TBD bool
  int priority)
  {
   BloadData(theEnv)->ClearBloadReadyFunctions =
      AddFunctionToCallList(theEnv,name,priority,
                            (void (*)(Environment *)) func,
                            BloadData(theEnv)->ClearBloadReadyFunctions);
  }

/*********************************************/
/* AddAbortBloadFunction: Adds a function to */
/*   the list of functions called if a bload */
/*   has to be aborted.                      */
/*********************************************/
void AddAbortBloadFunction(
  Environment *theEnv,
  const char *name,
  void (*func)(Environment *),
  int priority)
  {
   BloadData(theEnv)->AbortBloadFunctions = AddFunctionToCallList(theEnv,name,priority,func,BloadData(theEnv)->AbortBloadFunctions);
  }

/*******************************************************
  NAME         : BloadOutOfMemoryFunction
  DESCRIPTION  : Memory function used by bload to
                   prevent exiting when out of
                   memory - used by BloadandRefresh
  INPUTS       : The memory request size (unused)
  RETURNS      : True (indicates a failure and for
                 the memory functions to simply
                 return a NULL pointer)
  SIDE EFFECTS : None
  NOTES        : None
 *******************************************************/
static bool BloadOutOfMemoryFunction(
  Environment *theEnv,
  size_t size)
  {
#if MAC_XCD
#pragma unused(size,theEnv)
#endif
   return true;
  }

/*****************************************************/
/* CannotLoadWithBloadMessage: Generic error message */
/*   for indicating that a construct can't be loaded */
/*   when a binary image is active.                  */
/*****************************************************/
void CannotLoadWithBloadMessage(
  Environment *theEnv,
  const char *constructName)
  {
   PrintErrorID(theEnv,"BLOAD",1,true);
   EnvPrintRouter(theEnv,WERROR,"Cannot load ");
   EnvPrintRouter(theEnv,WERROR,constructName);
   EnvPrintRouter(theEnv,WERROR," construct with binary load in effect.\n");
  }

#endif /* (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) */

/**************************************/
/* BloadCommand: H/L access routine   */
/*   for the bload command.           */
/**************************************/
void BloadCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
#if (! RUN_TIME) && (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)
   const char *fileName;

   fileName = GetFileName(context);
   if (fileName != NULL)
     {
      mCVSetBoolean(returnValue,EnvBload(theEnv,fileName));
      return;
     }
#else
#if MAC_XCD
#pragma unused(size,context)
#endif
#endif
   mCVSetBoolean(returnValue,false);
  }
