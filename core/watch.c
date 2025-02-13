   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*                    WATCH MODULE                     */
   /*******************************************************/


/*************************************************************/
/* Purpose: Support functions for the watch and unwatch      */
/*   commands.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian Dantes                                         */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Added EnvSetWatchItem function.                */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Changed return values for router functions.    */
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

#if DEBUGGING_FUNCTIONS

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constant.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "router.h"

#include "watch.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static struct watchItem       *ValidWatchItem(Environment *,const char *,bool *);
   static bool                    RecognizeWatchRouters(Environment *,const char *);
   static void                    CaptureWatchPrints(Environment *,const char *,const char *);
   static void                    DeallocateWatchData(Environment *);

/**********************************************/
/* InitializeWatchData: Allocates environment */
/*    data for watch items.                   */
/**********************************************/
void InitializeWatchData(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,WATCH_DATA,sizeof(struct watchData),DeallocateWatchData);
  }
  
/************************************************/
/* DeallocateWatchData: Deallocates environment */
/*    data for watch items.                     */
/************************************************/
static void DeallocateWatchData(
  Environment *theEnv)
  {
   struct watchItem *tmpPtr, *nextPtr;

   tmpPtr = WatchData(theEnv)->ListOfWatchItems;
   while (tmpPtr != NULL)
     {
      nextPtr = tmpPtr->next;
      rtn_struct(theEnv,watchItem,tmpPtr);
      tmpPtr = nextPtr;
     }
  }

/*************************************************************/
/* AddWatchItem: Adds an item to the list of watchable items */
/*   that can be set using the watch and unwatch commands.   */
/*   Returns false if the item is already in the list,       */
/*   otherwise returns true.                                 */
/*************************************************************/
bool AddWatchItem(
  Environment *theEnv,
  const char *name,
  int code,
  bool *flag,
  int priority,
  bool (*accessFunc)(Environment *,int,bool,struct expr *),
  bool (*printFunc)(Environment *,const char *,int,struct expr *))
  {
   struct watchItem *newPtr, *currentPtr, *lastPtr;

   /*================================================================*/
   /* Find the insertion point in the watchable items list to place  */
   /* the new item. If the item is already in the list return false. */
   /*================================================================*/

   for (currentPtr = WatchData(theEnv)->ListOfWatchItems, lastPtr = NULL;
        currentPtr != NULL;
        currentPtr = currentPtr->next)
     {
      if (strcmp(currentPtr->name,name) == 0) return false;
      if (priority < currentPtr->priority) lastPtr = currentPtr;
     }

   /*============================*/
   /* Create the new watch item. */
   /*============================*/

   newPtr = get_struct(theEnv,watchItem);
   newPtr->name = name;
   newPtr->flag = flag;
   newPtr->code = code;
   newPtr->priority = priority;
   newPtr->accessFunc = accessFunc;
   newPtr->printFunc = printFunc;

   /*=================================================*/
   /* Insert the new item in the list of watch items. */
   /*=================================================*/

   if (lastPtr == NULL)
     {
      newPtr->next = WatchData(theEnv)->ListOfWatchItems;
      WatchData(theEnv)->ListOfWatchItems = newPtr;
     }
   else
     {
      newPtr->next = lastPtr->next;
      lastPtr->next = newPtr;
     }

   /*==================================================*/
   /* Return true to indicate the item has been added. */
   /*==================================================*/

   return true;
  }

/*****************************************************/
/* EnvWatch: C access routine for the watch command. */
/*****************************************************/
bool EnvWatch(
  Environment *theEnv,
  const char *itemName)
  {
   return(EnvSetWatchItem(theEnv,itemName,true,NULL));
  }

/*********************************************************/
/* EnvUnwatch: C access routine for the unwatch command. */
/*********************************************************/
bool EnvUnwatch(
  Environment *theEnv,
  const char *itemName)
  {
   return(EnvSetWatchItem(theEnv,itemName,false,NULL));
  }

/***********************************************************************/
/* EnvSetWatchItem: Sets the state of a specified watch item to either */
/*   on or off. Returns true if the item was set, otherwise false.     */
/***********************************************************************/
bool EnvSetWatchItem(
  Environment *theEnv,
  const char *itemName,
  bool newState,
  struct expr *argExprs)
  {
   struct watchItem *wPtr;

   /*===================================================*/
   /* If the name of the watch item to set is all, then */
   /* all watch items are set to the new state and true */
   /* is returned.                                      */
   /*===================================================*/

   if (strcmp(itemName,"all") == 0)
     {
      for (wPtr = WatchData(theEnv)->ListOfWatchItems; wPtr != NULL; wPtr = wPtr->next)
        {
         /*==============================================*/
         /* If no specific arguments are specified, then */
         /* set the global flag for the watch item.      */
         /*==============================================*/

         if (argExprs == NULL) *(wPtr->flag) = newState;

         /*=======================================*/
         /* Set flags for individual watch items. */
         /*=======================================*/

         if ((wPtr->accessFunc == NULL) ? false :
             ((*wPtr->accessFunc)(theEnv,wPtr->code,newState,argExprs) == false))
           {
            EnvSetEvaluationError(theEnv,true);
            return false;
           }
        }
      return true;
     }

   /*=================================================*/
   /* Search for the watch item to be set in the list */
   /* of watch items. If found, set the watch item to */
   /* its new state and return true.                  */
   /*=================================================*/

   for (wPtr = WatchData(theEnv)->ListOfWatchItems; wPtr != NULL; wPtr = wPtr->next)
     {
      if (strcmp(itemName,wPtr->name) == 0)
        {
         /*==============================================*/
         /* If no specific arguments are specified, then */
         /* set the global flag for the watch item.      */
         /*==============================================*/

         if (argExprs == NULL) *(wPtr->flag) = newState;

         /*=======================================*/
         /* Set flags for individual watch items. */
         /*=======================================*/

         if ((wPtr->accessFunc == NULL) ? false :
             ((*wPtr->accessFunc)(theEnv,wPtr->code,newState,argExprs) == false))
           {
            EnvSetEvaluationError(theEnv,true);
            return false;
           }

         return true;
        }
     }

   /*=================================================*/
   /* If the specified item was not found in the list */
   /* of watchable items then return false.           */
   /*=================================================*/

   return false;
  }

/******************************************************************/
/* EnvGetWatchItem: Gets the current state of the specified watch */
/*   item. Returns the state of the watch item (0 for off and 1   */
/*   for on) if the watch item is found in the list of watch      */
/*   items, otherwise -1 is returned.                             */
/******************************************************************/
int EnvGetWatchItem(
  Environment *theEnv,
  const char *itemName)
  {
   struct watchItem *wPtr;

   for (wPtr = WatchData(theEnv)->ListOfWatchItems; wPtr != NULL; wPtr = wPtr->next)
     { 
      if (strcmp(itemName,wPtr->name) == 0) 
        { return((int) *(wPtr->flag)); }
     }

   return(-1);
  }

/***************************************************************/
/* ValidWatchItem: Returns true if the specified name is found */
/*   in the list of watch items, otherwise returns false.      */
/***************************************************************/
static struct watchItem *ValidWatchItem(
  Environment *theEnv,
  const char *itemName,
  bool *recognized)
  {
   struct watchItem *wPtr;

   *recognized = true;
   if (strcmp(itemName,"all") == 0)
     return NULL;

   for (wPtr = WatchData(theEnv)->ListOfWatchItems; wPtr != NULL; wPtr = wPtr->next)
     { if (strcmp(itemName,wPtr->name) == 0) return(wPtr); }

   *recognized = false;
   return NULL;
  }

/*************************************************************/
/* GetNthWatchName: Returns the name associated with the nth */
/*   item in the list of watchable items. If the nth item    */
/*   does not exist, then NULL is returned.                  */
/*************************************************************/
const char *GetNthWatchName(
  Environment *theEnv,
  int whichItem)
  {
   int i;
   struct watchItem *wPtr;

   for (wPtr = WatchData(theEnv)->ListOfWatchItems, i = 1;
        wPtr != NULL;
        wPtr = wPtr->next, i++)
     { if (i == whichItem) return(wPtr->name); }

   return NULL;
  }

/***************************************************************/
/* GetNthWatchValue: Returns the current state associated with */
/*   the nth item in the list of watchable items. If the nth   */
/*   item does not exist, then -1 is returned.                 */
/***************************************************************/
int GetNthWatchValue(
  Environment *theEnv,
  int whichItem)
  {
   int i;
   struct watchItem *wPtr;

   for (wPtr = WatchData(theEnv)->ListOfWatchItems, i = 1;
        wPtr != NULL;
        wPtr = wPtr->next, i++)
     { if (i == whichItem) return((int) *(wPtr->flag)); }

   return(-1);
  }

/**************************************/
/* WatchCommand: H/L access routine   */
/*   for the watch command.           */
/**************************************/
void WatchCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theValue;
   const char *argument;
   bool recognized;
   struct watchItem *wPtr;

   /*========================================*/
   /* Determine which item is to be watched. */
   /*========================================*/

   if (! UDFFirstArgument(context,SYMBOL_TYPE,&theValue)) return;
      
   argument = mCVToString(&theValue);
   wPtr = ValidWatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      EnvSetEvaluationError(theEnv,true);
      UDFInvalidArgumentMessage(context,"watchable symbol");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if (GetNextArgument(GetFirstArgument()) != NULL)
     {
      if ((wPtr == NULL) ? true : (wPtr->accessFunc == NULL))
        {
         EnvSetEvaluationError(theEnv,true);
         ExpectedCountError(theEnv,"watch",EXACTLY,1);
         return;
        }
     }

   /*=====================*/
   /* Set the watch item. */
   /*=====================*/

   EnvSetWatchItem(theEnv,argument,true,GetNextArgument(GetFirstArgument()));
  }

/****************************************/
/* UnwatchCommand: H/L access routine   */
/*   for the unwatch command.           */
/****************************************/
void UnwatchCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theValue;
   const char *argument;
   bool recognized;
   struct watchItem *wPtr;

   /*==========================================*/
   /* Determine which item is to be unwatched. */
   /*==========================================*/

   if (! UDFFirstArgument(context,SYMBOL_TYPE,&theValue)) return;
   
   argument = mCVToString(&theValue);
   wPtr = ValidWatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      EnvSetEvaluationError(theEnv,true);
      UDFInvalidArgumentMessage(context,"watchable symbol");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if (GetNextArgument(GetFirstArgument()) != NULL)
     {
      if ((wPtr == NULL) ? true : (wPtr->accessFunc == NULL))
        {
         EnvSetEvaluationError(theEnv,true);
         ExpectedCountError(theEnv,"unwatch",EXACTLY,1);
         return;
        }
     }

   /*=====================*/
   /* Set the watch item. */
   /*=====================*/

   EnvSetWatchItem(theEnv,argument,false,GetNextArgument(GetFirstArgument()));
  }

/************************************************/
/* ListWatchItemsCommand: H/L access routines   */
/*   for the list-watch-items command.          */
/************************************************/
void ListWatchItemsCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   struct watchItem *wPtr;
   CLIPSValue theValue;
   bool recognized;

   /*=======================*/
   /* List the watch items. */
   /*=======================*/

   if (GetFirstArgument() == NULL)
     {
      for (wPtr = WatchData(theEnv)->ListOfWatchItems; wPtr != NULL; wPtr = wPtr->next)
        {
         EnvPrintRouter(theEnv,WDISPLAY,wPtr->name);
         if (*(wPtr->flag)) EnvPrintRouter(theEnv,WDISPLAY," = on\n");
         else EnvPrintRouter(theEnv,WDISPLAY," = off\n");
        }
      return;
     }

   /*=======================================*/
   /* Determine which item is to be listed. */
   /*=======================================*/

   if (! UDFFirstArgument(context,SYMBOL_TYPE,&theValue)) return;
   wPtr = ValidWatchItem(theEnv,mCVToString(&theValue),&recognized);
   if ((recognized == false) || (wPtr == NULL))
     {
      EnvSetEvaluationError(theEnv,true);
      ExpectedTypeError1(theEnv,"list-watch-items",1,"watchable symbol");
      return;
     }

   /*=================================================*/
   /* Check to make sure extra arguments are allowed. */
   /*=================================================*/

   if ((wPtr->printFunc == NULL) &&
       (GetNextArgument(GetFirstArgument()) != NULL))
     {
      EnvSetEvaluationError(theEnv,true);
      ExpectedCountError(theEnv,"list-watch-items",EXACTLY,1);
      return;
     }

   /*====================================*/
   /* List the status of the watch item. */
   /*====================================*/

   EnvPrintRouter(theEnv,WDISPLAY,wPtr->name);
   if (*(wPtr->flag)) EnvPrintRouter(theEnv,WDISPLAY," = on\n");
   else EnvPrintRouter(theEnv,WDISPLAY," = off\n");

   /*============================================*/
   /* List the status of individual watch items. */
   /*============================================*/

   if (wPtr->printFunc != NULL)
     {
      if ((*wPtr->printFunc)(theEnv,WDISPLAY,wPtr->code,
                             GetNextArgument(GetFirstArgument())) == false)
        { EnvSetEvaluationError(theEnv,true); }
     }
  }

/*******************************************/
/* GetWatchItemCommand: H/L access routine */
/*   for the get-watch-item command.       */
/*******************************************/
void GetWatchItemCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theValue;
   const char *argument;
   bool recognized;

   /*========================================*/
   /* Determine which item is to be watched. */
   /*========================================*/

   if (! UDFFirstArgument(context,SYMBOL_TYPE,&theValue))
     { return; }

   argument = mCVToString(&theValue);
   ValidWatchItem(theEnv,argument,&recognized);
   if (recognized == false)
     {
      EnvSetEvaluationError(theEnv,true);
      ExpectedTypeError1(theEnv,"get-watch-item",1,"watchable symbol");
      mCVSetBoolean(returnValue,false);
      return;
     }

   /*===========================*/
   /* Get the watch item value. */
   /*===========================*/

   if (EnvGetWatchItem(theEnv,argument) == 1)
     { mCVSetBoolean(returnValue,true); }

   mCVSetBoolean(returnValue,false);
  }

/*************************************************************/
/* WatchFunctionDefinitions: Initializes the watch commands. */
/*************************************************************/
void WatchFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   EnvAddUDF(theEnv,"watch","v",1,UNBOUNDED,"*;y",WatchCommand,"WatchCommand",NULL);
   EnvAddUDF(theEnv,"unwatch","v",1,UNBOUNDED,"*;y",UnwatchCommand,"UnwatchCommand",NULL);
   EnvAddUDF(theEnv,"get-watch-item","b",1,1,"y",GetWatchItemCommand,"GetWatchItemCommand",NULL);
   EnvAddUDF(theEnv,"list-watch-items","v",0,UNBOUNDED,"*;y",ListWatchItemsCommand,"ListWatchItemsCommand",NULL);
#endif

   EnvAddRouter(theEnv,WTRACE,1000,RecognizeWatchRouters,CaptureWatchPrints,NULL,NULL,NULL);
   EnvDeactivateRouter(theEnv,WTRACE);
  }

/**************************************************/
/* RecognizeWatchRouters: Looks for WTRACE prints */
/**************************************************/
static bool RecognizeWatchRouters(
  Environment *theEnv,
  const char *logName)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif

   if (strcmp(logName,WTRACE) == 0) return true;

   return false;
  }

/**************************************************/
/* CaptureWatchPrints: Suppresses WTRACE messages */
/**************************************************/
static void CaptureWatchPrints(
  Environment *theEnv,
  const char *logName,
  const char *str)
  {
#if MAC_XCD
#pragma unused(logName)
#pragma unused(str)
#pragma unused(theEnv)
#endif
  }

#endif /* DEBUGGING_FUNCTIONS */

