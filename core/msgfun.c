   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*              OBJECT MESSAGE FUNCTIONS               */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Changed integer type/precision.                */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
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
/*            UDF redesign.                                  */
/*                                                           */
/*      6.50: Option printing of carriage return for the     */
/*            SlotVisibilityViolationError function.         */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include "classcom.h"
#include "classfun.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "insfun.h"
#include "memalloc.h"
#include "msgcom.h"
#include "prccode.h"
#include "router.h"

#include "msgfun.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if DEBUGGING_FUNCTIONS
   static HANDLER_LINK           *DisplayPrimaryCore(Environment *,const char *,HANDLER_LINK *,int);
   static void                    PrintPreviewHandler(Environment *,const char *,HANDLER_LINK *,int,const char *);
#endif

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/********************************************************
  NAME         : UnboundHandlerErr
  DESCRIPTION  : Print out a synopis of the currently
                   executing handler for unbound variable
                   errors
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error synopsis printed to WERROR
  NOTES        : None
 ********************************************************/
void UnboundHandlerErr(
  Environment *theEnv)
  {
   EnvPrintRouter(theEnv,WERROR,"message-handler ");
   PrintHandler(theEnv,WERROR,MessageHandlerData(theEnv)->CurrentCore->hnd,true);
  }

/*****************************************************************
  NAME         : PrintNoHandlerError
  DESCRIPTION  : Print "No primaries found" error message for send
  INPUTS       : The name of the message
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************************/
void PrintNoHandlerError(
  Environment *theEnv,
  const char *msg)
  {
   PrintErrorID(theEnv,"MSGFUN",1,false);
   EnvPrintRouter(theEnv,WERROR,"No applicable primary message-handlers found for ");
   EnvPrintRouter(theEnv,WERROR,msg);
   EnvPrintRouter(theEnv,WERROR,".\n");
  }

/***************************************************************
  NAME         : CheckHandlerArgCount
  DESCRIPTION  : Verifies that the current argument
                   list satisfies the current
                   handler's parameter count restriction
  INPUTS       : None
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : EvaluationError set on errors
  NOTES        : Uses ProcParamArraySize and CurrentCore globals
 ***************************************************************/
bool CheckHandlerArgCount(
  Environment *theEnv)
  {
   DefmessageHandler *hnd;

   hnd = MessageHandlerData(theEnv)->CurrentCore->hnd;
   if ((hnd->maxParams == -1) ? (ProceduralPrimitiveData(theEnv)->ProcParamArraySize < hnd->minParams) :
       (ProceduralPrimitiveData(theEnv)->ProcParamArraySize != hnd->minParams))
     {
      EnvSetEvaluationError(theEnv,true);
      PrintErrorID(theEnv,"MSGFUN",2,false);
      EnvPrintRouter(theEnv,WERROR,"Message-handler ");
      EnvPrintRouter(theEnv,WERROR,ValueToString(hnd->name));
      EnvPrintRouter(theEnv,WERROR," ");
      EnvPrintRouter(theEnv,WERROR,MessageHandlerData(theEnv)->hndquals[hnd->type]);
      EnvPrintRouter(theEnv,WERROR," in class ");
      EnvPrintRouter(theEnv,WERROR,EnvGetDefclassName(theEnv,hnd->cls));
      EnvPrintRouter(theEnv,WERROR," expected ");
      if (hnd->maxParams == -1)
        EnvPrintRouter(theEnv,WERROR,"at least ");
      else
        EnvPrintRouter(theEnv,WERROR,"exactly ");
      PrintLongInteger(theEnv,WERROR,(long long) (hnd->minParams-1));
      EnvPrintRouter(theEnv,WERROR," argument(s).\n");
      return false;
     }
   return true;
  }

/***************************************************
  NAME         : SlotAccessViolationError
  DESCRIPTION  : Prints out an error message when
                 attempt is made to set a read-only
                 or initialize-only slot improperly
  INPUTS       : 1) The slot name
                 2) A flag indicating if the source
                    is a class or an instance
                 3) A pointer to the source
                    instance/class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
void SlotAccessViolationError(
  Environment *theEnv,
  const char *slotName,
  bool instanceFlag,
  void *theInstanceOrClass)
  {
   PrintErrorID(theEnv,"MSGFUN",3,false);
   EnvPrintRouter(theEnv,WERROR,slotName);
   EnvPrintRouter(theEnv,WERROR," slot in ");
   if (instanceFlag)
     PrintInstanceNameAndClass(theEnv,WERROR,(Instance *) theInstanceOrClass,false);
   else
     {
      EnvPrintRouter(theEnv,WERROR,"class ");
      PrintClassName(theEnv,WERROR,(Defclass *) theInstanceOrClass,false);
     }
   EnvPrintRouter(theEnv,WERROR,": write access denied.\n");
  }

/***************************************************
  NAME         : SlotVisibilityViolationError
  DESCRIPTION  : Prints out an error message when
                 attempt is made to access a
                 private slot improperly
  INPUTS       : 1) The slot descriptor
                 2) A pointer to the source class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ***************************************************/
void SlotVisibilityViolationError(
  Environment *theEnv,
  SlotDescriptor *sd,
  Defclass *theDefclass,
  bool printCR)
  {
   PrintErrorID(theEnv,"MSGFUN",6,printCR);
   EnvPrintRouter(theEnv,WERROR,"Private slot ");
   EnvPrintRouter(theEnv,WERROR,ValueToString(sd->slotName->name));
   EnvPrintRouter(theEnv,WERROR," of class ");
   PrintClassName(theEnv,WERROR,sd->cls,false);
   EnvPrintRouter(theEnv,WERROR," cannot be accessed directly\n   by handlers attached to class ");
   PrintClassName(theEnv,WERROR,theDefclass,true);
  }

#if ! RUN_TIME

/******************************************************************************
  NAME         : NewSystemHandler
  DESCRIPTION  : Adds a new system handler for a system class

                 The handler is assumed to be primary and of
                 the form:

                 (defmessage-handler <class> <handler> () (<func>))

  INPUTS       : 1) Name-string of the system class
                 2) Name-string of the system handler
                 3) Name-string of the internal H/L function to implement
                      this handler
                 4) The number of extra arguments (past the instance itself)
                    that the handler willl accept
  RETURNS      : Nothing useful
  SIDE EFFECTS : Creates the new handler and inserts it in the system class's
                   handler array
                 On errors, generate a system error and exits.
  NOTES        : Does not check to see if handler already exists
 *******************************************************************************/
void NewSystemHandler(
  Environment *theEnv,
  const char *cname,
  const char *mname,
  const char *fname,
  int extraargs)
  {
   Defclass *cls;
   DefmessageHandler *hnd;

   cls = LookupDefclassInScope(theEnv,cname);
   hnd = InsertHandlerHeader(theEnv,cls,(SYMBOL_HN *) EnvAddSymbol(theEnv,mname),MPRIMARY);
   IncrementSymbolCount(hnd->name);
   hnd->system = 1;
   hnd->minParams = hnd->maxParams = (short) (extraargs + 1);
   hnd->localVarCount = 0;
   hnd->actions = get_struct(theEnv,expr);
   hnd->actions->argList = NULL;
   hnd->actions->type = FCALL;
   hnd->actions->value = FindFunction(theEnv,fname);
   hnd->actions->nextArg = NULL;
  }

/***************************************************
  NAME         : InsertHandlerHeader
  DESCRIPTION  : Allocates a new handler header and
                   inserts it in the proper (sorted)
                   position in the class hnd array
  INPUTS       : 1) The class
                 2) The handler name
                 3) The handler type
  RETURNS      : The address of the new handler
                   header, NULL on errors
  SIDE EFFECTS : Class handler array reallocated
                   and resorted
  NOTES        : Assumes handler does not exist
 ***************************************************/
DefmessageHandler *InsertHandlerHeader(
  Environment *theEnv,
  Defclass *cls,
  SYMBOL_HN *mname,
  int mtype)
  {
   DefmessageHandler *nhnd,*hnd;
   unsigned *narr,*arr;
   long i;
   long j,ni = -1;

   hnd = cls->handlers;
   arr = cls->handlerOrderMap;
   nhnd = (DefmessageHandler *) gm2(theEnv,(sizeof(DefmessageHandler) * (cls->handlerCount+1)));
   narr = (unsigned *) gm2(theEnv,(sizeof(unsigned) * (cls->handlerCount+1)));
   GenCopyMemory(DefmessageHandler,cls->handlerCount,nhnd,hnd);
   for (i = 0 , j = 0 ; i < cls->handlerCount ; i++ , j++)
     {
      if (ni == -1)
        {
         if ((hnd[arr[i]].name->bucket > mname->bucket) ? true :
             (hnd[arr[i]].name == mname))
           {
            ni = i;
            j++;
           }
        }
      narr[j] = arr[i];
     }
   if (ni == -1)
     ni = (int) cls->handlerCount;
   narr[ni] = cls->handlerCount;
   nhnd[cls->handlerCount].system = 0;
   nhnd[cls->handlerCount].type = mtype;
   nhnd[cls->handlerCount].busy = 0;
   nhnd[cls->handlerCount].mark = 0;
#if DEBUGGING_FUNCTIONS
   nhnd[cls->handlerCount].trace = MessageHandlerData(theEnv)->WatchHandlers;
#endif
   nhnd[cls->handlerCount].name = mname;
   nhnd[cls->handlerCount].cls = cls;
   nhnd[cls->handlerCount].minParams = 0;
   nhnd[cls->handlerCount].maxParams = 0;
   nhnd[cls->handlerCount].localVarCount = 0;
   nhnd[cls->handlerCount].actions = NULL;
   nhnd[cls->handlerCount].ppForm = NULL;
   nhnd[cls->handlerCount].usrData = NULL;
   if (cls->handlerCount != 0)
     {
      rm(theEnv,hnd,(sizeof(DefmessageHandler) * cls->handlerCount));
      rm(theEnv,arr,(sizeof(unsigned) * cls->handlerCount));
     }
   cls->handlers = nhnd;
   cls->handlerOrderMap = narr;
   cls->handlerCount++;
   return(&nhnd[cls->handlerCount-1]);
  }

#endif

#if (! BLOAD_ONLY) && (! RUN_TIME)

/*****************************************************
  NAME         : HandlersExecuting
  DESCRIPTION  : Determines if any message-handlers
                   for a class are currently executing
  INPUTS       : The class address
  RETURNS      : True if any handlers are executing,
                   false otherwise
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
bool HandlersExecuting(
  Defclass *cls)
  {
   long i;

   for (i = 0 ; i < cls->handlerCount ; i++)
     if (cls->handlers[i].busy > 0)
       return true;
   return false;
  }

/*********************************************************************
  NAME         : DeleteHandler
  DESCRIPTION  : Deletes one or more message-handlers
                   from a class definition
  INPUTS       : 1) The class address
                 2) The message-handler name
                    (if this is * and there is no handler
                     called *, then the delete operations
                     will be applied to all handlers matching the type
                 3) The message-handler type
                    (if this is -1, then the delete operations will be
                     applied to all handlers matching the name
                 4) A flag saying whether to print error messages when
                     handlers are not found meeting specs
  RETURNS      : 1 if successful, 0 otherwise
  SIDE EFFECTS : Handlers deleted
  NOTES        : If any handlers for the class are
                   currently executing, this routine
                   will fail
 **********************************************************************/
bool DeleteHandler(
   Environment *theEnv,
   Defclass *cls,
   SYMBOL_HN *mname,
   int mtype,
   bool indicate_missing)
  {
   long i;
   DefmessageHandler *hnd;
   bool found,success = true;

   if (cls->handlerCount == 0)
     {
      if (indicate_missing)
        {
         HandlerDeleteError(theEnv,EnvGetDefclassName(theEnv,cls));
         return false;
        }
      return true;
     }
   if (HandlersExecuting(cls))
     {
      HandlerDeleteError(theEnv,EnvGetDefclassName(theEnv,cls));
      return false;
     }
   if (mtype == -1)
     {
      found = false;
      for (i = MAROUND ; i <= MAFTER ; i++)
        {
         hnd = FindHandlerByAddress(cls,mname,(unsigned) i);
         if (hnd != NULL)
           {
            found = true;
            if (hnd->system == 0)
              hnd->mark = 1;
            else
              {
               PrintErrorID(theEnv,"MSGPSR",3,false);
               EnvPrintRouter(theEnv,WERROR,"System message-handlers may not be modified.\n");
               success = false;
              }
           }
        }
      if ((found == false) ? (strcmp(ValueToString(mname),"*") == 0) : false)
        {
         for (i = 0 ; i < cls->handlerCount ; i++)
           if (cls->handlers[i].system == 0)
             cls->handlers[i].mark = 1;
        }
     }
   else
     {
      hnd = FindHandlerByAddress(cls,mname,(unsigned) mtype);
      if (hnd == NULL)
        {
         if (strcmp(ValueToString(mname),"*") == 0)
           {
            for (i = 0 ; i < cls->handlerCount ; i++)
              if ((cls->handlers[i].type == (unsigned) mtype) &&
                  (cls->handlers[i].system == 0))
                cls->handlers[i].mark = 1;
           }
         else
           {
            if (indicate_missing)
              HandlerDeleteError(theEnv,EnvGetDefclassName(theEnv,cls));
            success = false;
           }
        }
      else if (hnd->system == 0)
        hnd->mark = 1;
      else
        {
         if (indicate_missing)
           {
            PrintErrorID(theEnv,"MSGPSR",3,false);
            EnvPrintRouter(theEnv,WERROR,"System message-handlers may not be modified.\n");
           }
         success = false;
        }
     }
   DeallocateMarkedHandlers(theEnv,cls);
   return(success);
  }

/***************************************************
  NAME         : DeallocateMarkedHandlers
  DESCRIPTION  : Removes any handlers from a class
                   that have been previously marked
                   for deletion.
  INPUTS       : The class
  RETURNS      : Nothing useful
  SIDE EFFECTS : Marked handlers are deleted
  NOTES        : Assumes none of the handlers are
                   currently executing or have a
                   busy count != 0 for any reason
 ***************************************************/
void DeallocateMarkedHandlers(
  Environment *theEnv,
  Defclass *cls)
  {
   short count;
   DefmessageHandler *hnd,*nhnd;
   unsigned *arr,*narr;
   long i,j;

   for (i = 0 , count = 0 ; i < cls->handlerCount ; i++)
     {
      hnd = &cls->handlers[i];
      if (hnd->mark == 1)
        {
         count++;
         DecrementSymbolCount(theEnv,hnd->name);
         ExpressionDeinstall(theEnv,hnd->actions);
         ReturnPackedExpression(theEnv,hnd->actions);
         ClearUserDataList(theEnv,hnd->usrData);
         if (hnd->ppForm != NULL)
           rm(theEnv,hnd->ppForm,
              (sizeof(char) * (strlen(hnd->ppForm)+1)));
        }
      else
         /* ============================================
            Use the busy field to count how many
            message-handlers are removed before this one
            ============================================ */
        hnd->busy = count;
     }
   if (count == 0)
     return;
   if (count == cls->handlerCount)
     {
      rm(theEnv,cls->handlers,(sizeof(DefmessageHandler) * cls->handlerCount));
      rm(theEnv,cls->handlerOrderMap,(sizeof(unsigned) * cls->handlerCount));
      cls->handlers = NULL;
      cls->handlerOrderMap = NULL;
      cls->handlerCount = 0;
     }
   else
     {
      count = (short) (cls->handlerCount - count);
      hnd = cls->handlers;
      arr = cls->handlerOrderMap;
      nhnd = (DefmessageHandler *) gm2(theEnv,(sizeof(DefmessageHandler) * count));
      narr = (unsigned *) gm2(theEnv,(sizeof(unsigned) * count));
      for (i = 0 , j = 0 ; j < count ; i++)
        {
         if (hnd[arr[i]].mark == 0)
           {
            /* ==============================================================
               The offsets in the map need to be decremented by the number of
               preceding nodes which were deleted.  Use the value of the busy
               field set in the first loop.
               ============================================================== */
            narr[j] = arr[i] - hnd[arr[i]].busy;
            j++;
           }
        }
      for (i = 0 , j = 0 ; j < count ; i++)
        {
         if (hnd[i].mark == 0)
           {
            hnd[i].busy = 0;
            GenCopyMemory(DefmessageHandler,1,&nhnd[j],&hnd[i]);
            j++;
           }
        }
      rm(theEnv,hnd,(sizeof(DefmessageHandler) * cls->handlerCount));
      rm(theEnv,arr,(sizeof(unsigned) * cls->handlerCount));
      cls->handlers = nhnd;
      cls->handlerOrderMap = narr;
      cls->handlerCount = count;
     }
  }

#endif

/*****************************************************
  NAME         : HandlerType
  DESCRIPTION  : Determines type of message-handler
  INPUTS       : 1) Calling function string
                 2) String representing type
  RETURNS      : MAROUND  (0) for "around"
                 MBEFORE  (1) for "before"
                 MPRIMARY (2) for "primary"
                 MAFTER   (3) for "after"
                 MERROR   (4) on errors
  SIDE EFFECTS : None
  NOTES        : None
 *****************************************************/
unsigned HandlerType(
  Environment *theEnv,
  const char *func,
  const char *str)
  {
   unsigned i;

   for (i = MAROUND ; i <= MAFTER ; i++)
     if (strcmp(str,MessageHandlerData(theEnv)->hndquals[i]) == 0)
       {
        return(i);
       }

   PrintErrorID(theEnv,"MSGFUN",7,false);
   EnvPrintRouter(theEnv,"werror","Unrecognized message-handler type in ");
   EnvPrintRouter(theEnv,"werror",func);
   EnvPrintRouter(theEnv,"werror",".\n");
   return(MERROR);
  }

/*****************************************************************
  NAME         : CheckCurrentMessage
  DESCRIPTION  : Makes sure that a message is available
                   and active for an internal message function
  INPUTS       : 1) The name of the function checking the message
                 2) A flag indicating whether the object must be
                      a class instance or not (it could be a
                      primitive type)
  RETURNS      : True if all OK, false otherwise
  SIDE EFFECTS : EvaluationError set on errors
  NOTES        : None
 *****************************************************************/
bool CheckCurrentMessage(
  Environment *theEnv,
  const char *func,
  bool ins_reqd)
  {
   CLIPSValue *activeMsgArg;

   if (!MessageHandlerData(theEnv)->CurrentCore || (MessageHandlerData(theEnv)->CurrentCore->hnd->actions != ProceduralPrimitiveData(theEnv)->CurrentProcActions))
     {
      PrintErrorID(theEnv,"MSGFUN",4,false);
      EnvPrintRouter(theEnv,WERROR,func);
      EnvPrintRouter(theEnv,WERROR," may only be called from within message-handlers.\n");
      EnvSetEvaluationError(theEnv,true);
      return false;
     }
   activeMsgArg = GetNthMessageArgument(theEnv,0);
   if ((ins_reqd == true) ? (activeMsgArg->type != INSTANCE_ADDRESS) : false)
     {
      PrintErrorID(theEnv,"MSGFUN",5,false);
      EnvPrintRouter(theEnv,WERROR,func);
      EnvPrintRouter(theEnv,WERROR," operates only on instances.\n");
      EnvSetEvaluationError(theEnv,true);
      return false;
     }
   if ((activeMsgArg->type == INSTANCE_ADDRESS) ?
       (((Instance *) activeMsgArg->value)->garbage == 1) : false)
     {
      StaleInstanceAddress(theEnv,func,0);
      EnvSetEvaluationError(theEnv,true);
      return false;
     }
   return true;
  }

/***************************************************
  NAME         : PrintHandler
  DESCRIPTION  : Displays a handler synopsis
  INPUTS       : 1) Logical name of output
                 2) The handler
                 5) Flag indicating whether to
                    printout a terminating newline
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void PrintHandler(
  Environment *theEnv,
  const char *logName,
  DefmessageHandler *theHandler,
  bool crtn)
  {
   EnvPrintRouter(theEnv,logName,ValueToString(theHandler->name));
   EnvPrintRouter(theEnv,logName," ");
   EnvPrintRouter(theEnv,logName,MessageHandlerData(theEnv)->hndquals[theHandler->type]);
   EnvPrintRouter(theEnv,logName," in class ");
   PrintClassName(theEnv,logName,theHandler->cls,crtn);
  }

/***********************************************************
  NAME         : FindHandlerByAddress
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
                 3) The handler type (MPRIMARY,etc.)
  RETURNS      : The address of the found handler,
                   NULL if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: symbolic name of handler
                   2nd key: type of handler
 ***********************************************************/
DefmessageHandler *FindHandlerByAddress(
  Defclass *cls,
  SYMBOL_HN *name,
  unsigned type)
  {
   int b;
   long i;
   DefmessageHandler *hnd;
   unsigned *arr;

   if ((b = FindHandlerNameGroup(cls,name)) == -1)
     return NULL;
   arr = cls->handlerOrderMap;
   hnd = cls->handlers;
   for (i = (unsigned) b ; i < cls->handlerCount ; i++)
     {
      if (hnd[arr[i]].name != name)
        return NULL;
      if (hnd[arr[i]].type == type)
        return(&hnd[arr[i]]);
     }
   return NULL;
  }

/***********************************************************
  NAME         : FindHandlerByIndex
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
                 3) The handler type (MPRIMARY,etc.)
  RETURNS      : The index of the found handler,
                   -1 if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: symbolic name of handler
                   2nd key: type of handler
 ***********************************************************/
int FindHandlerByIndex(
  Defclass *cls,
  SYMBOL_HN *name,
  unsigned type)
  {
   int b;
   long i;
   DefmessageHandler *hnd;
   unsigned *arr;

   if ((b = FindHandlerNameGroup(cls,name)) == -1)
     return(-1);
   arr = cls->handlerOrderMap;
   hnd = cls->handlers;
   for (i = (unsigned) b ; i < cls->handlerCount ; i++)
     {
      if (hnd[arr[i]].name != name)
        return(-1);
      if (hnd[arr[i]].type == type)
        return((int) arr[i]);
     }
   return(-1);
  }

/*****************************************************
  NAME         : FindHandlerNameGroup
  DESCRIPTION  : Uses a binary search on a class's
                   handler header array
  INPUTS       : 1) The class address
                 2) The handler symbolic name
  RETURNS      : The index of the found handler group
                   -1 if not found
  SIDE EFFECTS : None
  NOTES        : Assumes array is in ascending order
                   1st key: handler name symbol bucket
 *****************************************************/
int FindHandlerNameGroup(
  Defclass *cls,
  SYMBOL_HN *name)
  {
   int b,e,i,j;
   DefmessageHandler *hnd;
   unsigned *arr;
   int start;

   if (cls->handlerCount == 0)
     return(-1);
   hnd = cls->handlers;
   arr = cls->handlerOrderMap;
   b = 0;
   e = (int) (cls->handlerCount-1);
   start = -1;
   do
     {
      i = (b+e)/2;
      if (name->bucket == hnd[arr[i]].name->bucket)
        {
         for (j = i ; j >= b ; j--)
           {
            if (hnd[arr[j]].name == name)
              start = j;
            if (hnd[arr[j]].name->bucket != name->bucket)
              break;
           }
         if (start != -1)
           return(start);
         for (j = i+1 ; j <= e ; j++)
           {
            if (hnd[arr[j]].name == name)
              return(j);
            if (hnd[arr[j]].name->bucket != name->bucket)
              return(-1);
           }
         return(-1);
        }
      else if (name->bucket < hnd[arr[i]].name->bucket)
        e = i-1;
      else
        b = i+1;
     }
   while (b <= e);
   return(-1);
  }

/***************************************************
  NAME         : HandlerDeleteError
  DESCRIPTION  : Prints out an error message when
                   handlers cannot be deleted
  INPUTS       : Name-string of the class
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
void HandlerDeleteError(
  Environment *theEnv,
  const char *cname)
  {
   PrintErrorID(theEnv,"MSGFUN",8,false);
   EnvPrintRouter(theEnv,WERROR,"Unable to delete message-handler(s) from class ");
   EnvPrintRouter(theEnv,WERROR,cname);
   EnvPrintRouter(theEnv,WERROR,".\n");
  }

#if DEBUGGING_FUNCTIONS

/********************************************************************
  NAME         : DisplayCore
  DESCRIPTION  : Gives a schematic "printout" of the
                   core framework for a message showing
                   arounds, primaries, shadows etc.
                 This routine uses recursion to print indentation
                   to indicate shadowing and where handlers begin
                   and end execution wrt one another.
  INPUTS       : 1) Logical name of output
                 2) The remaining core
                 3) The number of handlers this (partial) core
                    shadows
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Expects that the core was created in PREVIEW mode,
                   i.e. implicit handlers are SlotDescriptor addresses
                   (in PERFORM mode they are INSTANCE_SLOT addresses)
                 Assumes (partial) core is not empty
 ********************************************************************/
void DisplayCore(
  Environment *theEnv,
  const char *logicalName,
  HANDLER_LINK *core,
  int sdepth)
  {
   if (core->hnd->type == MAROUND)
     {
      PrintPreviewHandler(theEnv,logicalName,core,sdepth,BEGIN_TRACE);
      if (core->nxt != NULL)
        DisplayCore(theEnv,logicalName,core->nxt,sdepth+1);
      PrintPreviewHandler(theEnv,logicalName,core,sdepth,END_TRACE);
     }
   else
     {
      while ((core != NULL) ? (core->hnd->type == MBEFORE) : false)
        {
         PrintPreviewHandler(theEnv,logicalName,core,sdepth,BEGIN_TRACE);
         PrintPreviewHandler(theEnv,logicalName,core,sdepth,END_TRACE);
         core = core->nxt;
        }
      if ((core != NULL) ? (core->hnd->type == MPRIMARY) : false)

        core = DisplayPrimaryCore(theEnv,logicalName,core,sdepth);

      while ((core != NULL) ? (core->hnd->type == MAFTER) : false)
        {
         PrintPreviewHandler(theEnv,logicalName,core,sdepth,BEGIN_TRACE);
         PrintPreviewHandler(theEnv,logicalName,core,sdepth,END_TRACE);
         core = core->nxt;
        }

     }
  }

/*******************************************************************
  NAME         : FindPreviewApplicableHandlers
  DESCRIPTION  : See FindApplicableHandlers
                 However, this function only examines classes rather
                   than instances for implicit slot-accessors
  INPUTS       : 1) The class address
                 2) The message name symbol
  RETURNS      : The links of applicable handlers, NULL on errors
  SIDE EFFECTS : Links are allocated for the list
  NOTES        : None
 ******************************************************************/
HANDLER_LINK *FindPreviewApplicableHandlers(
  Environment *theEnv,
  Defclass *cls,
  SYMBOL_HN *mname)
  {
   int i;
   HANDLER_LINK *tops[4],*bots[4];

   for (i = MAROUND ; i <= MAFTER ; i++)
     tops[i] = bots[i] = NULL;

   for (i = 0 ; i < cls->allSuperclasses.classCount ; i++)
     FindApplicableOfName(theEnv,cls->allSuperclasses.classArray[i],tops,bots,mname);
   return(JoinHandlerLinks(theEnv,tops,bots,mname));
  }

/***********************************************************
  NAME         : WatchMessage
  DESCRIPTION  : Prints a condensed description of a
                   message and its arguments
  INPUTS       : 1) The output logical name
                 2) BEGIN_TRACE or END_TRACE string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the global variables ProcParamArray
                   and CurrentMessageName
 ***********************************************************/
void WatchMessage(
  Environment *theEnv,
  const char *logName,
  const char *tstring)
  {
   EnvPrintRouter(theEnv,logName,"MSG ");
   EnvPrintRouter(theEnv,logName,tstring);
   EnvPrintRouter(theEnv,logName," ");
   EnvPrintRouter(theEnv,logName,ValueToString(MessageHandlerData(theEnv)->CurrentMessageName));
   EnvPrintRouter(theEnv,logName," ED:");
   PrintLongInteger(theEnv,logName,(long long) EvaluationData(theEnv)->CurrentEvaluationDepth);
   PrintProcParamArray(theEnv,logName);
  }

/***********************************************************
  NAME         : WatchHandler
  DESCRIPTION  : Prints a condensed description of a
                   message-handler and its arguments
  INPUTS       : 1) The output logical name
                 2) The handler address
                 3) BEGIN_TRACE or END_TRACE string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : Uses the global variables ProcParamArray
                   and CurrentMessageName
 ***********************************************************/
void WatchHandler(
  Environment *theEnv,
  const char *logName,
  HANDLER_LINK *hndl,
  const char *tstring)
  {
   DefmessageHandler *hnd;
   
   EnvPrintRouter(theEnv,logName,"HND ");
   EnvPrintRouter(theEnv,logName,tstring);
   EnvPrintRouter(theEnv,logName," ");
   hnd = hndl->hnd;
   PrintHandler(theEnv,WTRACE,hnd,true);
   EnvPrintRouter(theEnv,logName,"       ED:");
   PrintLongInteger(theEnv,logName,(long long) EvaluationData(theEnv)->CurrentEvaluationDepth);
   PrintProcParamArray(theEnv,logName);
  }

#endif /* DEBUGGING_FUNCTIONS */

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if DEBUGGING_FUNCTIONS

/********************************************************************
  NAME         : DisplayPrimaryCore
  DESCRIPTION  : Gives a schematic "printout" of the primary
                   message showing other shadowed primaries
                 This routine uses recursion to print indentation
                   to indicate shadowing and where handlers begin
                   and end execution wrt one another.
  INPUTS       : 1) The logical name of the output
                 2) The remaining core
                 3) The number of handlers this (partial) core
                    shadows
  RETURNS      : The address of the handler following the primary
                   group of handlers in the core
  SIDE EFFECTS : None
  NOTES        : Expects that the core was created in PREVIEW mode,
                   i.e. implicit handlers are SlotDescriptor addresses
                   (in PERFORM mode they are INSTANCE_SLOT addresses)
                 Assumes (partial) core is not empty
 ********************************************************************/
static HANDLER_LINK *DisplayPrimaryCore(
  Environment *theEnv,
  const char *logicalName,
  HANDLER_LINK *core,
  int pdepth)
  {
   HANDLER_LINK *rtn;

   PrintPreviewHandler(theEnv,logicalName,core,pdepth,BEGIN_TRACE);
   if ((core->nxt != NULL) ? (core->nxt->hnd->type == MPRIMARY) : false)
     rtn = DisplayPrimaryCore(theEnv,logicalName,core->nxt,pdepth+1);
   else
     rtn = core->nxt;
   PrintPreviewHandler(theEnv,logicalName,core,pdepth,END_TRACE);
   return(rtn);
  }

/***************************************************
  NAME         : PrintPreviewHandler
  DESCRIPTION  : Displays a message preview
  INPUTS       : 1) The logical name of the output
                 2) Handler-link
                 3) Number of handlers shadowed
                 4) The trace-string
  RETURNS      : Nothing useful
  SIDE EFFECTS : None
  NOTES        : None
 ***************************************************/
static void PrintPreviewHandler(
  Environment *theEnv,
  const char *logicalName,
  HANDLER_LINK *cptr,
  int sdepth,
  const char *tstr)
  {
   int i;

   for (i = 0 ; i < sdepth ; i++)
     EnvPrintRouter(theEnv,logicalName,"| ");
   EnvPrintRouter(theEnv,logicalName,tstr);
   EnvPrintRouter(theEnv,logicalName," ");
   PrintHandler(theEnv,logicalName,cptr->hnd,true);
  }

#endif

#endif

