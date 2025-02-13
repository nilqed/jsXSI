   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*             OBJECT MESSAGE DISPATCH CODE            */
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
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Removed IMPERATIVE_MESSAGE_HANDLERS and        */
/*            AUXILIARY_MESSAGE_HANDLERS compilation flags.  */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: The return value of DirectMessage indicates    */
/*            whether an execution error has occurred.       */
/*                                                           */
/*            Removed conditional code for unsupported       */
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
/*            It's no longer necessary for a defclass to be  */
/*            in scope in order to sent a message to an      */
/*            instance of that class.                        */
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

#include <stdio.h>
#include <stdlib.h>

#include "argacces.h"
#include "classcom.h"
#include "classfun.h"
#include "commline.h"
#include "constrct.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "inscom.h"
#include "insfun.h"
#include "memalloc.h"
#include "msgcom.h"
#include "msgfun.h"
#include "multifld.h"
#include "prccode.h"
#include "prcdrfun.h"
#include "proflfun.h"
#include "router.h"
#include "strngfun.h"
#include "utility.h"

#include "msgpass.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    PerformMessage(Environment *,CLIPSValue *,EXPRESSION *,SYMBOL_HN *);
   static HANDLER_LINK           *FindApplicableHandlers(Environment *,Defclass *,SYMBOL_HN *);
   static void                    CallHandlers(Environment *,CLIPSValue *);
   static void                    EarlySlotBindError(Environment *,Instance *,Defclass *,unsigned);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*****************************************************
  NAME         : DirectMessage
  DESCRIPTION  : Plugs in given instance and
                  performs specified message
  INPUTS       : 1) Message symbolic name
                 2) The instance address
                 3) Address of CLIPSValue buffer
                    (NULL if don't care)
                 4) Message argument expressions
  RETURNS      : Returns false is an execution error occurred
                 or execution is halted, otherwise true
  SIDE EFFECTS : Side effects of message execution
  NOTES        : None
 *****************************************************/
bool DirectMessage(
  Environment *theEnv,
  SYMBOL_HN *msg,
  Instance *ins,
  CLIPSValue *resultbuf,
  EXPRESSION *remargs)
  {
   EXPRESSION args;
   CLIPSValue temp;

   if (resultbuf == NULL)
     resultbuf = &temp;
     
   args.nextArg = remargs;
   args.argList = NULL;
   args.type = INSTANCE_ADDRESS;
   args.value = ins;
   
   return PerformMessage(theEnv,resultbuf,&args,msg);
  }

/***************************************************
  NAME         : EnvSend
  DESCRIPTION  : C Interface for sending messages to
                  instances
  INPUTS       : 1) The data object of the instance
                 2) The message name-string
                 3) The message arguments string
                    (Constants only)
                 4) Caller's buffer for result
  RETURNS      : Nothing useful
  SIDE EFFECTS : Executes message and stores result
                   caller's buffer
  NOTES        : None
 ***************************************************/
void EnvSend(
  Environment *theEnv,
  CLIPSValue *idata,
  const char *msg,
  const char *args,
  CLIPSValue *returnValue)
  {
   bool error;
   EXPRESSION *iexp;
   SYMBOL_HN *msym;

   if ((UtilityData(theEnv)->CurrentGarbageFrame->topLevel) && (! CommandLineData(theEnv)->EvaluatingTopLevelCommand) &&
       (EvaluationData(theEnv)->CurrentExpression == NULL) && (UtilityData(theEnv)->GarbageCollectionLocks == 0))
     {
      CleanCurrentGarbageFrame(theEnv,NULL);
      CallPeriodicTasks(theEnv);
     }

   EnvSetEvaluationError(theEnv,false);
   returnValue->type = SYMBOL;
   returnValue->value = EnvFalseSymbol(theEnv);
   msym = FindSymbolHN(theEnv,msg);
   if (msym == NULL)
     {
      PrintNoHandlerError(theEnv,msg);
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   iexp = GenConstant(theEnv,idata->type,idata->value);
   iexp->nextArg = ParseConstantArguments(theEnv,args,&error);
   if (error == true)
     {
      ReturnExpression(theEnv,iexp);
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   PerformMessage(theEnv,returnValue,iexp,msym);
   ReturnExpression(theEnv,iexp);
  }

/*****************************************************
  NAME         : DestroyHandlerLinks
  DESCRIPTION  : Iteratively deallocates handler-links
  INPUTS       : The handler-link list
  RETURNS      : Nothing useful
  SIDE EFFECTS : Deallocation of links
  NOTES        : None
 *****************************************************/
void DestroyHandlerLinks(
  Environment *theEnv,
  HANDLER_LINK *mhead)
  {
   HANDLER_LINK *tmp;

   while (mhead != NULL)
     {
      tmp = mhead;
      mhead = mhead->nxt;
      tmp->hnd->busy--;
      DecrementDefclassBusyCount(theEnv,tmp->hnd->cls);
      rtn_struct(theEnv,messageHandlerLink,tmp);
     }
  }

/***********************************************************************
  NAME         : SendCommand
  DESCRIPTION  : Determines the applicable handler(s) and sets up the
                   core calling frame.  Then calls the core frame.
  INPUTS       : Caller's space for storing the result of the handler(s)
  RETURNS      : Nothing useful
  SIDE EFFECTS : Any side-effects caused by the execution of handlers in
                   the core framework
  NOTES        : H/L Syntax : (send <instance> <hnd> <args>*)
 ***********************************************************************/
void SendCommand(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   EXPRESSION args;
   SYMBOL_HN *msg;
   CLIPSValue theArg;

   mCVSetBoolean(returnValue,false);

   if (! UDFNthArgument(context,2,SYMBOL_TYPE,&theArg)) return;
   msg = (SYMBOL_HN *) theArg.value;

   /* =============================================
      Get the instance or primitive for the message
      ============================================= */
   args.type = GetFirstArgument()->type;
   args.value = GetFirstArgument()->value;
   args.argList = GetFirstArgument()->argList;
   args.nextArg = GetFirstArgument()->nextArg->nextArg;

   PerformMessage(theEnv,returnValue,&args,msg);
  }

/***************************************************
  NAME         : GetNthMessageArgument
  DESCRIPTION  : Returns the address of the nth
                 (starting at 1) which is an
                 argument of the current message
                 dispatch
  INPUTS       : None
  RETURNS      : The message argument
  SIDE EFFECTS : None
  NOTES        : The active instance is always
                 stored as the first argument (0) in
                 the call frame of the message
 ***************************************************/
CLIPSValue *GetNthMessageArgument(
  Environment *theEnv,
  int n)
  {
   return(&ProceduralPrimitiveData(theEnv)->ProcParamArray[n]);
  }

/********************************/
/* NextHandlerAvailableFunction */
/********************************/
void NextHandlerAvailableFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   mCVSetBoolean(returnValue,NextHandlerAvailable(theEnv));
  }

/*****************************************************
  NAME         : NextHandlerAvailable
  DESCRIPTION  : Determines if there the currently
                   executing handler can call a
                   shadowed handler
                 Used before calling call-next-handler
  INPUTS       : None
  RETURNS      : True if shadow ready, false otherwise
  SIDE EFFECTS : None
  NOTES        : H/L Syntax: (next-handlerp)
 *****************************************************/
bool NextHandlerAvailable(
  Environment *theEnv)
  {
   if (MessageHandlerData(theEnv)->CurrentCore == NULL)
     { return false; }
   
   if (MessageHandlerData(theEnv)->CurrentCore->hnd->type == MAROUND)
     { return (MessageHandlerData(theEnv)->NextInCore != NULL) ? true : false; }
   
   if ((MessageHandlerData(theEnv)->CurrentCore->hnd->type == MPRIMARY) &&
       (MessageHandlerData(theEnv)->NextInCore != NULL))
     { return (MessageHandlerData(theEnv)->NextInCore->hnd->type == MPRIMARY) ? true : false; }
   
   return false;
  }

/********************************************************
  NAME         : CallNextHandler
  DESCRIPTION  : This function allows around-handlers
                   to execute the rest of the core frame.
                 It also allows primary handlers
                   to execute shadowed primaries.

                 The original handler arguments are
                   left intact.
  INPUTS       : The caller's result-value buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : The core frame is called and any
                   appropriate changes are made when
                   used in an around handler
                   See CallHandlers()
                 But when call-next-handler is called
                   from a primary, the same shadowed
                   primary is called over and over
                   again for repeated calls to
                   call-next-handler.
  NOTES        : H/L Syntax: (call-next-handler) OR
                    (override-next-handler <arg> ...)
 ********************************************************/
void CallNextHandler(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   EXPRESSION args;
   int overridep;
   HANDLER_LINK *oldNext,*oldCurrent;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif

   mCVSetBoolean(returnValue,false);
   
   EvaluationData(theEnv)->EvaluationError = false;
   if (EvaluationData(theEnv)->HaltExecution)
     return;
   if (NextHandlerAvailable(theEnv) == false)
     {
      PrintErrorID(theEnv,"MSGPASS",1,false);
      EnvPrintRouter(theEnv,WERROR,"Shadowed message-handlers not applicable in current context.\n");
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   if (EvaluationData(theEnv)->CurrentExpression->value == (void *) FindFunction(theEnv,"override-next-handler"))
     {
      overridep = 1;
      args.type = ProceduralPrimitiveData(theEnv)->ProcParamArray[0].type;
      if (args.type != MULTIFIELD)
        args.value = ProceduralPrimitiveData(theEnv)->ProcParamArray[0].value;
      else
        args.value = &ProceduralPrimitiveData(theEnv)->ProcParamArray[0];
      args.nextArg = GetFirstArgument();
      args.argList = NULL;
      PushProcParameters(theEnv,&args,CountArguments(&args),
                          ValueToString(MessageHandlerData(theEnv)->CurrentMessageName),"message",
                          UnboundHandlerErr);
      if (EvaluationData(theEnv)->EvaluationError)
        {
         ProcedureFunctionData(theEnv)->ReturnFlag = false;
         return;
        }
     }
   else
     overridep = 0;
   oldNext = MessageHandlerData(theEnv)->NextInCore;
   oldCurrent = MessageHandlerData(theEnv)->CurrentCore;
   if (MessageHandlerData(theEnv)->CurrentCore->hnd->type == MAROUND)
     {
      if (MessageHandlerData(theEnv)->NextInCore->hnd->type == MAROUND)
        {
         MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->NextInCore;
         MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
           WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
         if (CheckHandlerArgCount(theEnv))
           {
#if PROFILING_FUNCTIONS
            StartProfile(theEnv,&profileFrame,
                         &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                         ProfileFunctionData(theEnv)->ProfileConstructs);
#endif

            EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                               MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                               MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                               returnValue,UnboundHandlerErr);
#if PROFILING_FUNCTIONS
            EndProfile(theEnv,&profileFrame);
#endif
           }
#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
           WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
#endif
        }
      else
        CallHandlers(theEnv,returnValue);
     }
   else
     {
      MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->NextInCore;
      MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
      if (CheckHandlerArgCount(theEnv))
        {
#if PROFILING_FUNCTIONS
        StartProfile(theEnv,&profileFrame,
                     &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                     ProfileFunctionData(theEnv)->ProfileConstructs);
#endif

        EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                            returnValue,UnboundHandlerErr);
#if PROFILING_FUNCTIONS
         EndProfile(theEnv,&profileFrame);
#endif
        }

#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
#endif
     }
   MessageHandlerData(theEnv)->NextInCore = oldNext;
   MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
   if (overridep)
     PopProcParameters(theEnv);
   ProcedureFunctionData(theEnv)->ReturnFlag = false;
  }

/*************************************************************************
  NAME         : FindApplicableOfName
  DESCRIPTION  : Groups all handlers of all types of the specified
                   class of the specified name into the applicable handler
                   list
  INPUTS       : 1) The class address
                 2-3) The tops and bottoms of the four handler type lists:
                      around, before, primary and after
                 4) The message name symbol
  RETURNS      : Nothing useful
  SIDE EFFECTS : Modifies the handler lists to include applicable handlers
  NOTES        : None
 *************************************************************************/
void FindApplicableOfName(
  Environment *theEnv,
  Defclass *cls,
  HANDLER_LINK *tops[4],
  HANDLER_LINK *bots[4],
  SYMBOL_HN *mname)
  {
   int i;
   int e;
   DefmessageHandler *hnd;
   unsigned *arr;
   HANDLER_LINK *tmp;

   i = FindHandlerNameGroup(cls,mname);
   if (i == -1)
     return;
   e = ((int) cls->handlerCount) - 1;
   hnd = cls->handlers;
   arr = cls->handlerOrderMap;
   for ( ; i <= e ; i++)
     {
      if (hnd[arr[i]].name != mname)
        break;

      tmp = get_struct(theEnv,messageHandlerLink);
      hnd[arr[i]].busy++;
      IncrementDefclassBusyCount(theEnv,hnd[arr[i]].cls);
      tmp->hnd = &hnd[arr[i]];
      if (tops[tmp->hnd->type] == NULL)
        {
         tmp->nxt = NULL;
         tops[tmp->hnd->type] = bots[tmp->hnd->type] = tmp;
        }

      else if (tmp->hnd->type == MAFTER)
        {
         tmp->nxt = tops[tmp->hnd->type];
         tops[tmp->hnd->type] = tmp;
        }

      else
        {
         bots[tmp->hnd->type]->nxt = tmp;
         bots[tmp->hnd->type] = tmp;
         tmp->nxt = NULL;
        }
     }
  }

/*************************************************************************
  NAME         : JoinHandlerLinks
  DESCRIPTION  : Joins the queues of different handlers together
  INPUTS       : 1-2) The tops and bottoms of the four handler type lists:
                      around, before, primary and after
                 3) The message name symbol
  RETURNS      : The top of the joined lists, NULL on errors
  SIDE EFFECTS : Links all the handler type lists together, or all the
                   lists are destroyed if there are no primary handlers
  NOTES        : None
 *************************************************************************/
HANDLER_LINK *JoinHandlerLinks(
  Environment *theEnv,
  HANDLER_LINK *tops[4],
  HANDLER_LINK *bots[4],
  SYMBOL_HN *mname)
  {
   int i;
   HANDLER_LINK *mlink;

   if (tops[MPRIMARY] == NULL)
    {
     PrintNoHandlerError(theEnv,ValueToString(mname));
     for (i = MAROUND ; i <= MAFTER ; i++)
       DestroyHandlerLinks(theEnv,tops[i]);
     EnvSetEvaluationError(theEnv,true);
     return NULL;
    }

   mlink = tops[MPRIMARY];

   if (tops[MBEFORE] != NULL)
     {
      bots[MBEFORE]->nxt = mlink;
      mlink = tops[MBEFORE];
     }

   if (tops[MAROUND] != NULL)
     {
      bots[MAROUND]->nxt = mlink;
      mlink = tops[MAROUND];
     }

   bots[MPRIMARY]->nxt = tops[MAFTER];

   return(mlink);
  }

/***************************************************
  NAME         : PrintHandlerSlotGetFunction
  DESCRIPTION  : Developer access function for
                 printing direct slot references
                 in message-handlers
  INPUTS       : 1) The logical name of the output
                 2) The bitmap expression
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expression printed
  NOTES        : None
 ***************************************************/
void PrintHandlerSlotGetFunction(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   HANDLER_SLOT_REFERENCE *theReference;
   Defclass *theDefclass;
   SlotDescriptor *sd;

   theReference = (HANDLER_SLOT_REFERENCE *) ValueToBitMap(theValue);
   EnvPrintRouter(theEnv,logicalName,"?self:[");
   theDefclass = DefclassData(theEnv)->ClassIDMap[theReference->classID];
   EnvPrintRouter(theEnv,logicalName,ValueToString(theDefclass->header.name));
   EnvPrintRouter(theEnv,logicalName,"]");
   sd = theDefclass->instanceTemplate[theDefclass->slotNameMap[theReference->slotID] - 1];
   EnvPrintRouter(theEnv,logicalName,ValueToString(sd->slotName->name));
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************************
  NAME         : HandlerSlotGetFunction
  DESCRIPTION  : Access function for handling the
                 statically-bound direct slot
                 references in message-handlers
  INPUTS       : 1) The bitmap expression
                 2) A data object buffer
  RETURNS      : True if OK, false
                 on errors
  SIDE EFFECTS : Data object buffer gets value of
                 slot. On errors, buffer gets
                 symbol false, EvaluationError
                 is set and error messages are
                 printed
  NOTES        : It is possible for a handler
                 (attached to a superclass of
                  the currently active instance)
                 containing these static references
                 to be called for an instance
                 which does not contain the slots
                 (e.g., an instance of a subclass
                  where the original slot was
                  no-inherit or the subclass
                  overrode the original slot)
 ***************************************************/
bool HandlerSlotGetFunction(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   HANDLER_SLOT_REFERENCE *theReference;
   Defclass *theDefclass;
   Instance *theInstance;
   INSTANCE_SLOT *sp;
   unsigned instanceSlotIndex;

   theReference = (HANDLER_SLOT_REFERENCE *) ValueToBitMap(theValue);
   theInstance = (Instance *) ProceduralPrimitiveData(theEnv)->ProcParamArray[0].value;
   theDefclass = DefclassData(theEnv)->ClassIDMap[theReference->classID];

   if (theInstance->garbage)
     {
      StaleInstanceAddress(theEnv,"for slot get",0);
      theResult->type = SYMBOL;
      theResult->value = EnvFalseSymbol(theEnv);
      EnvSetEvaluationError(theEnv,true);
      return false;
     }

   if (theInstance->cls == theDefclass)
     {
      instanceSlotIndex = theInstance->cls->slotNameMap[theReference->slotID];
      sp = theInstance->slotAddresses[instanceSlotIndex - 1];
     }
   else
     {
      if (theReference->slotID > theInstance->cls->maxSlotNameID)
        goto HandlerGetError;
      instanceSlotIndex = theInstance->cls->slotNameMap[theReference->slotID];
      if (instanceSlotIndex == 0)
        goto HandlerGetError;
      instanceSlotIndex--;
      sp = theInstance->slotAddresses[instanceSlotIndex];
      if (sp->desc->cls != theDefclass)
        goto HandlerGetError;
     }
   theResult->type = (unsigned short) sp->type;
   theResult->value = sp->value;
   if (sp->type == MULTIFIELD)
     {
      theResult->begin = 0;
      SetpDOEnd(theResult,GetInstanceSlotLength(sp));
     }
   return true;

HandlerGetError:
   EarlySlotBindError(theEnv,theInstance,theDefclass,theReference->slotID);
   theResult->type = SYMBOL;
   theResult->value = EnvFalseSymbol(theEnv);
   EnvSetEvaluationError(theEnv,true);
   return false;
  }

/***************************************************
  NAME         : PrintHandlerSlotPutFunction
  DESCRIPTION  : Developer access function for
                 printing direct slot bindings
                 in message-handlers
  INPUTS       : 1) The logical name of the output
                 2) The bitmap expression
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expression printed
  NOTES        : None
 ***************************************************/
void PrintHandlerSlotPutFunction(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   HANDLER_SLOT_REFERENCE *theReference;
   Defclass *theDefclass;
   SlotDescriptor *sd;

   theReference = (HANDLER_SLOT_REFERENCE *) ValueToBitMap(theValue);
   EnvPrintRouter(theEnv,logicalName,"(bind ?self:[");
   theDefclass = DefclassData(theEnv)->ClassIDMap[theReference->classID];
   EnvPrintRouter(theEnv,logicalName,ValueToString(theDefclass->header.name));
   EnvPrintRouter(theEnv,logicalName,"]");
   sd = theDefclass->instanceTemplate[theDefclass->slotNameMap[theReference->slotID] - 1];
   EnvPrintRouter(theEnv,logicalName,ValueToString(sd->slotName->name));
   if (GetFirstArgument() != NULL)
     {
      EnvPrintRouter(theEnv,logicalName," ");
      PrintExpression(theEnv,logicalName,GetFirstArgument());
     }
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

/***************************************************
  NAME         : HandlerSlotPutFunction
  DESCRIPTION  : Access function for handling the
                 statically-bound direct slot
                 bindings in message-handlers
  INPUTS       : 1) The bitmap expression
                 2) A data object buffer
  RETURNS      : True if OK, false
                 on errors
  SIDE EFFECTS : Data object buffer gets symbol
                 TRUE and slot is set. On errors,
                 buffer gets symbol FALSE,
                 EvaluationError is set and error
                 messages are printed
  NOTES        : It is possible for a handler
                 (attached to a superclass of
                  the currently active instance)
                 containing these static references
                 to be called for an instance
                 which does not contain the slots
                 (e.g., an instance of a subclass
                  where the original slot was
                  no-inherit or the subclass
                  overrode the original slot)
 ***************************************************/
bool HandlerSlotPutFunction(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   HANDLER_SLOT_REFERENCE *theReference;
   Defclass *theDefclass;
   Instance *theInstance;
   INSTANCE_SLOT *sp;
   unsigned instanceSlotIndex;
   CLIPSValue theSetVal;

   theReference = (HANDLER_SLOT_REFERENCE *) ValueToBitMap(theValue);
   theInstance = (Instance *) ProceduralPrimitiveData(theEnv)->ProcParamArray[0].value;
   theDefclass = DefclassData(theEnv)->ClassIDMap[theReference->classID];

   if (theInstance->garbage)
     {
      StaleInstanceAddress(theEnv,"for slot put",0);
      theResult->type = SYMBOL;
      theResult->value = EnvFalseSymbol(theEnv);
      EnvSetEvaluationError(theEnv,true);
      return false;
     }

   if (theInstance->cls == theDefclass)
     {
      instanceSlotIndex = theInstance->cls->slotNameMap[theReference->slotID];
      sp = theInstance->slotAddresses[instanceSlotIndex - 1];
     }
   else
     {
      if (theReference->slotID > theInstance->cls->maxSlotNameID)
        goto HandlerPutError;
      instanceSlotIndex = theInstance->cls->slotNameMap[theReference->slotID];
      if (instanceSlotIndex == 0)
        goto HandlerPutError;
      instanceSlotIndex--;
      sp = theInstance->slotAddresses[instanceSlotIndex];
      if (sp->desc->cls != theDefclass)
        goto HandlerPutError;
     }

   /* =======================================================
      The slot has already been verified not to be read-only.
      However, if it is initialize-only, we need to make sure
      that we are initializing the instance (something we
      could not verify at parse-time)
      ======================================================= */
   if (sp->desc->initializeOnly && (!theInstance->initializeInProgress))
     {
      SlotAccessViolationError(theEnv,ValueToString(sp->desc->slotName->name),
                               true,theInstance);
      goto HandlerPutError2;
     }

   /* ======================================
      No arguments means to use the
      special NoParamValue to reset the slot
      to its default value
      ====================================== */
   if (GetFirstArgument())
     {
      if (EvaluateAndStoreInDataObject(theEnv,(int) sp->desc->multiple,
                                       GetFirstArgument(),&theSetVal,true) == false)
         goto HandlerPutError2;
     }
   else
     {
      SetDOBegin(theSetVal,1);
      SetDOEnd(theSetVal,0);
      SetType(theSetVal,MULTIFIELD);
      SetValue(theSetVal,ProceduralPrimitiveData(theEnv)->NoParamValue);
     }
   if (PutSlotValue(theEnv,theInstance,sp,&theSetVal,theResult,NULL) == false)
      goto HandlerPutError2;
   return true;

HandlerPutError:
   EarlySlotBindError(theEnv,theInstance,theDefclass,theReference->slotID);

HandlerPutError2:
   theResult->type = SYMBOL;
   theResult->value = EnvFalseSymbol(theEnv);
   EnvSetEvaluationError(theEnv,true);

   return false;
  }

/*****************************************************
  NAME         : DynamicHandlerGetSlot
  DESCRIPTION  : Directly references a slot's value
                 (uses dynamic binding to lookup slot)
  INPUTS       : The caller's result buffer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Caller's result buffer set
  NOTES        : H/L Syntax: (get <slot>)
 *****************************************************/
void DynamicHandlerGetSlot(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   INSTANCE_SLOT *sp;
   Instance *ins;
   CLIPSValue temp;

   returnValue->type = SYMBOL;
   returnValue->value = EnvFalseSymbol(theEnv);
   if (CheckCurrentMessage(theEnv,"dynamic-get",true) == false)
     return;
   EvaluateExpression(theEnv,GetFirstArgument(),&temp);
   if (temp.type != SYMBOL)
     {
      ExpectedTypeError1(theEnv,"dynamic-get",1,"symbol");
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   ins = GetActiveInstance(theEnv);
   sp = FindInstanceSlot(theEnv,ins,(SYMBOL_HN *) temp.value);
   if (sp == NULL)
     {
      SlotExistError(theEnv,ValueToString(temp.value),"dynamic-get");
      return;
     }
   if ((sp->desc->publicVisibility == 0) &&
       (MessageHandlerData(theEnv)->CurrentCore->hnd->cls != sp->desc->cls))
     {
      SlotVisibilityViolationError(theEnv,sp->desc,MessageHandlerData(theEnv)->CurrentCore->hnd->cls,false);
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   returnValue->type = (unsigned short) sp->type;
   returnValue->value = sp->value;
   if (sp->type == MULTIFIELD)
     {
      returnValue->begin = 0;
      SetpDOEnd(returnValue,GetInstanceSlotLength(sp));
     }
  }

/***********************************************************
  NAME         : DynamicHandlerPutSlot
  DESCRIPTION  : Directly puts a slot's value
                 (uses dynamic binding to lookup slot)
  INPUTS       : Data obejct buffer for holding slot value
  RETURNS      : Nothing useful
  SIDE EFFECTS : Slot modified - and caller's buffer set
                 to value (or symbol FALSE on errors)
  NOTES        : H/L Syntax: (put <slot> <value>*)
 ***********************************************************/
void DynamicHandlerPutSlot(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   INSTANCE_SLOT *sp;
   Instance *ins;
   CLIPSValue temp;

   returnValue->type = SYMBOL;
   returnValue->value = EnvFalseSymbol(theEnv);
   if (CheckCurrentMessage(theEnv,"dynamic-put",true) == false)
     return;
   EvaluateExpression(theEnv,GetFirstArgument(),&temp);
   if (temp.type != SYMBOL)
     {
      ExpectedTypeError1(theEnv,"dynamic-put",1,"symbol");
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   ins = GetActiveInstance(theEnv);
   sp = FindInstanceSlot(theEnv,ins,(SYMBOL_HN *) temp.value);
   if (sp == NULL)
     {
      SlotExistError(theEnv,ValueToString(temp.value),"dynamic-put");
      return;
     }
   if ((sp->desc->noWrite == 0) ? false :
       ((sp->desc->initializeOnly == 0) || (!ins->initializeInProgress)))
     {
      SlotAccessViolationError(theEnv,ValueToString(sp->desc->slotName->name),
                               true,ins);
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   if ((sp->desc->publicVisibility == 0) &&
       (MessageHandlerData(theEnv)->CurrentCore->hnd->cls != sp->desc->cls))
     {
      SlotVisibilityViolationError(theEnv,sp->desc,MessageHandlerData(theEnv)->CurrentCore->hnd->cls,false);
      EnvSetEvaluationError(theEnv,true);
      return;
     }
   if (GetFirstArgument()->nextArg)
     {
      if (EvaluateAndStoreInDataObject(theEnv,(int) sp->desc->multiple,
                        GetFirstArgument()->nextArg,&temp,true) == false)
        return;
     }
   else
     {
      SetpDOBegin(&temp,1);
      SetpDOEnd(&temp,0);
      SetpType(&temp,MULTIFIELD);
      SetpValue(&temp,ProceduralPrimitiveData(theEnv)->NoParamValue);
     }
   PutSlotValue(theEnv,ins,sp,&temp,returnValue,NULL);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/*****************************************************
  NAME         : PerformMessage
  DESCRIPTION  : Calls core framework for a message
  INPUTS       : 1) Caller's result buffer
                 2) Message argument expressions
                    (including implicit object)
                 3) Message name
  RETURNS      : Returns false is an execution error occurred
                 or execution is halted, otherwise true
  SIDE EFFECTS : Any side-effects of message execution
                    and caller's result buffer set
  NOTES        : It's no longer necessary for a defclass
                 to be in scope in order to sent a message
                 to an instance of that class.
 *****************************************************/
static bool PerformMessage(
  Environment *theEnv,
  CLIPSValue *returnValue,
  EXPRESSION *args,
  SYMBOL_HN *mname)
  {
   int oldce;
   /* HANDLER_LINK *oldCore; */
   Defclass *cls = NULL;
   Instance *ins = NULL;
   SYMBOL_HN *oldName;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif
   struct CLIPSBlock gcBlock;
   
   returnValue->type = SYMBOL;
   returnValue->value = EnvFalseSymbol(theEnv);
   EvaluationData(theEnv)->EvaluationError = false;
   if (EvaluationData(theEnv)->HaltExecution)
     return false;

   CLIPSBlockStart(theEnv,&gcBlock);
   
   oldce = ExecutingConstruct(theEnv);
   SetExecutingConstruct(theEnv,true);
   oldName = MessageHandlerData(theEnv)->CurrentMessageName;
   MessageHandlerData(theEnv)->CurrentMessageName = mname;
   EvaluationData(theEnv)->CurrentEvaluationDepth++;

   PushProcParameters(theEnv,args,CountArguments(args),
                        ValueToString(MessageHandlerData(theEnv)->CurrentMessageName),"message",
                        UnboundHandlerErr);


   if (EvaluationData(theEnv)->EvaluationError)
     {
      EvaluationData(theEnv)->CurrentEvaluationDepth--;
      MessageHandlerData(theEnv)->CurrentMessageName = oldName;
      
      CLIPSBlockEnd(theEnv,&gcBlock,returnValue);
      CallPeriodicTasks(theEnv);
      
      SetExecutingConstruct(theEnv,oldce);
      return false;
     }

   if (ProceduralPrimitiveData(theEnv)->ProcParamArray->type == INSTANCE_ADDRESS)
     {
      ins = (Instance *) ProceduralPrimitiveData(theEnv)->ProcParamArray->value;
      if (ins->garbage == 1)
        {
         StaleInstanceAddress(theEnv,"send",0);
         EnvSetEvaluationError(theEnv,true);
        }
      //else if (DefclassInScope(theEnv,ins->cls,EnvGetCurrentModule(theEnv)) == false)
      //  NoInstanceError(theEnv,ValueToString(ins->name),"send");
      else
        {
         cls = ins->cls;
         ins->busy++;
        }
     }
   else if (ProceduralPrimitiveData(theEnv)->ProcParamArray->type == INSTANCE_NAME)
     {
      ins = FindInstanceBySymbol(theEnv,(SYMBOL_HN *) ProceduralPrimitiveData(theEnv)->ProcParamArray->value);
      if (ins == NULL)
        {
         PrintErrorID(theEnv,"MSGPASS",2,false);
         EnvPrintRouter(theEnv,WERROR,"No such instance ");
         EnvPrintRouter(theEnv,WERROR,ValueToString((SYMBOL_HN *) ProceduralPrimitiveData(theEnv)->ProcParamArray->value));
         EnvPrintRouter(theEnv,WERROR," in function send.\n");
         EnvSetEvaluationError(theEnv,true);
        }
      else
        {
         ProceduralPrimitiveData(theEnv)->ProcParamArray->value = ins;
         ProceduralPrimitiveData(theEnv)->ProcParamArray->type = INSTANCE_ADDRESS;
         cls = ins->cls;
         ins->busy++;
        }
     }
   else if ((cls = DefclassData(theEnv)->PrimitiveClassMap[ProceduralPrimitiveData(theEnv)->ProcParamArray->type]) == NULL)
     {
      SystemError(theEnv,"MSGPASS",1);
      EnvExitRouter(theEnv,EXIT_FAILURE);
     }
   if (EvaluationData(theEnv)->EvaluationError)
     {
      PopProcParameters(theEnv);
      EvaluationData(theEnv)->CurrentEvaluationDepth--;
      MessageHandlerData(theEnv)->CurrentMessageName = oldName;
         
      CLIPSBlockEnd(theEnv,&gcBlock,returnValue);
      CallPeriodicTasks(theEnv);

      SetExecutingConstruct(theEnv,oldce);
      return false;
     }

   /* oldCore = MessageHandlerData(theEnv)->TopOfCore; */

   if (MessageHandlerData(theEnv)->TopOfCore != NULL)
     { MessageHandlerData(theEnv)->TopOfCore->nxtInStack = MessageHandlerData(theEnv)->OldCore; }
   MessageHandlerData(theEnv)->OldCore = MessageHandlerData(theEnv)->TopOfCore;
   
   MessageHandlerData(theEnv)->TopOfCore = FindApplicableHandlers(theEnv,cls,mname);

   if (MessageHandlerData(theEnv)->TopOfCore != NULL)
     {
      HANDLER_LINK *oldCurrent,*oldNext;

      oldCurrent = MessageHandlerData(theEnv)->CurrentCore;
      oldNext = MessageHandlerData(theEnv)->NextInCore;

      if (MessageHandlerData(theEnv)->TopOfCore->hnd->type == MAROUND)
        {
         MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->TopOfCore;
         MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->TopOfCore->nxt;
#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->WatchMessages)
           WatchMessage(theEnv,WTRACE,BEGIN_TRACE);
         if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
           WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
         if (CheckHandlerArgCount(theEnv))
           {
#if PROFILING_FUNCTIONS
            StartProfile(theEnv,&profileFrame,
                         &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                         ProfileFunctionData(theEnv)->ProfileConstructs);
#endif


           EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                               MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                               MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                               returnValue,UnboundHandlerErr);


#if PROFILING_FUNCTIONS
            EndProfile(theEnv,&profileFrame);
#endif
           }

#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
           WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
         if (MessageHandlerData(theEnv)->WatchMessages)
           WatchMessage(theEnv,WTRACE,END_TRACE);
#endif
        }
      else
        {
         MessageHandlerData(theEnv)->CurrentCore = NULL;
         MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->TopOfCore;
#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->WatchMessages)
           WatchMessage(theEnv,WTRACE,BEGIN_TRACE);
#endif
         CallHandlers(theEnv,returnValue);
#if DEBUGGING_FUNCTIONS
         if (MessageHandlerData(theEnv)->WatchMessages)
           WatchMessage(theEnv,WTRACE,END_TRACE);
#endif
        }

      DestroyHandlerLinks(theEnv,MessageHandlerData(theEnv)->TopOfCore);
      MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
      MessageHandlerData(theEnv)->NextInCore = oldNext;
     }

   /* MessageHandlerData(theEnv)->TopOfCore = oldCore; */
   MessageHandlerData(theEnv)->TopOfCore = MessageHandlerData(theEnv)->OldCore;
   if (MessageHandlerData(theEnv)->OldCore != NULL)
     { MessageHandlerData(theEnv)->OldCore = MessageHandlerData(theEnv)->OldCore->nxtInStack; }

   ProcedureFunctionData(theEnv)->ReturnFlag = false;

   if (ins != NULL)
     ins->busy--;

   /* ==================================
      Restore the original calling frame
      ================================== */
   PopProcParameters(theEnv);
   EvaluationData(theEnv)->CurrentEvaluationDepth--;
   MessageHandlerData(theEnv)->CurrentMessageName = oldName;

   CLIPSBlockEnd(theEnv,&gcBlock,returnValue);
   CallPeriodicTasks(theEnv);

   SetExecutingConstruct(theEnv,oldce);

   if (EvaluationData(theEnv)->EvaluationError)
     {
      returnValue->type = SYMBOL;
      returnValue->value = EnvFalseSymbol(theEnv);
      return false;
     }
     
   return true;
  }

/*****************************************************************************
  NAME         : FindApplicableHandlers
  DESCRIPTION  : Given a message name, this routine forms the "core frame"
                   for the message : a list of all applicable class handlers.
                   An applicable class handler is one whose name matches
                     the message and whose class matches the instance.

                   The list is in the following order :

                   All around handlers (from most specific to most general)
                   All before handlers (from most specific to most general)
                   All primary handlers (from most specific to most general)
                   All after handlers (from most general to most specific)

  INPUTS       : 1) The class of the instance (or primitive) for the message
                 2) The message name
  RETURNS      : NULL if no applicable handlers or errors,
                   the list of handlers otherwise
  SIDE EFFECTS : Links are allocated for the list
  NOTES        : The instance is the first thing on the ProcParamArray
                 The number of arguments is in ProcParamArraySize
 *****************************************************************************/
static HANDLER_LINK *FindApplicableHandlers(
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

/***************************************************************
  NAME         : CallHandlers
  DESCRIPTION  : Moves though the current message frame
                   for a send-message as follows :

                 Call all before handlers and ignore their
                   return values.
                 Call the first primary handler and
                   ignore the rest.  The return value
                   of the handler frame is this message's value.
                 Call all after handlers and ignore their
                   return values.
  INPUTS       : Caller's buffer for the return value of
                   the message
  RETURNS      : Nothing useful
  SIDE EFFECTS : The handlers are evaluated.
  NOTES        : IMPORTANT : The global NextInCore should be
                 pointing to the first handler to be executed.
 ***************************************************************/
static void CallHandlers(
  Environment *theEnv,
  CLIPSValue *returnValue)
  {
   HANDLER_LINK *oldCurrent = NULL,*oldNext = NULL;  /* prevents warning */
   CLIPSValue temp;
#if PROFILING_FUNCTIONS
   struct profileFrameInfo profileFrame;
#endif

   if (EvaluationData(theEnv)->HaltExecution)
     return;

   oldCurrent = MessageHandlerData(theEnv)->CurrentCore;
   oldNext = MessageHandlerData(theEnv)->NextInCore;

   while (MessageHandlerData(theEnv)->NextInCore->hnd->type == MBEFORE)
     {
      MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->NextInCore;
      MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
      if (CheckHandlerArgCount(theEnv))
        {
#if PROFILING_FUNCTIONS
         StartProfile(theEnv,&profileFrame,
                      &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                      ProfileFunctionData(theEnv)->ProfileConstructs);
#endif

         EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                            &temp,UnboundHandlerErr);


#if PROFILING_FUNCTIONS
         EndProfile(theEnv,&profileFrame);
#endif
        }

#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
#endif
      ProcedureFunctionData(theEnv)->ReturnFlag = false;
      if ((MessageHandlerData(theEnv)->NextInCore == NULL) || EvaluationData(theEnv)->HaltExecution)
        {
         MessageHandlerData(theEnv)->NextInCore = oldNext;
         MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
         return;
        }
     }
   if (MessageHandlerData(theEnv)->NextInCore->hnd->type == MPRIMARY)
     {
      MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->NextInCore;
      MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
      if (CheckHandlerArgCount(theEnv))
        {
#if PROFILING_FUNCTIONS
         StartProfile(theEnv,&profileFrame,
                      &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                      ProfileFunctionData(theEnv)->ProfileConstructs);
#endif


        EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                            returnValue,UnboundHandlerErr);

#if PROFILING_FUNCTIONS
         EndProfile(theEnv,&profileFrame);
#endif
        }


#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
#endif
      ProcedureFunctionData(theEnv)->ReturnFlag = false;

      if ((MessageHandlerData(theEnv)->NextInCore == NULL) || EvaluationData(theEnv)->HaltExecution)
        {
         MessageHandlerData(theEnv)->NextInCore = oldNext;
         MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
         return;
        }
      while (MessageHandlerData(theEnv)->NextInCore->hnd->type == MPRIMARY)
        {
         MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
         if (MessageHandlerData(theEnv)->NextInCore == NULL)
           {
            MessageHandlerData(theEnv)->NextInCore = oldNext;
            MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
            return;
           }
        }
     }
   while (MessageHandlerData(theEnv)->NextInCore->hnd->type == MAFTER)
     {
      MessageHandlerData(theEnv)->CurrentCore = MessageHandlerData(theEnv)->NextInCore;
      MessageHandlerData(theEnv)->NextInCore = MessageHandlerData(theEnv)->NextInCore->nxt;
#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,BEGIN_TRACE);
#endif
      if (CheckHandlerArgCount(theEnv))
        {
#if PROFILING_FUNCTIONS
         StartProfile(theEnv,&profileFrame,
                      &MessageHandlerData(theEnv)->CurrentCore->hnd->usrData,
                      ProfileFunctionData(theEnv)->ProfileConstructs);
#endif


         EvaluateProcActions(theEnv,MessageHandlerData(theEnv)->CurrentCore->hnd->cls->header.whichModule->theModule,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->actions,
                            MessageHandlerData(theEnv)->CurrentCore->hnd->localVarCount,
                            &temp,UnboundHandlerErr);

#if PROFILING_FUNCTIONS
         EndProfile(theEnv,&profileFrame);
#endif
        }


#if DEBUGGING_FUNCTIONS
      if (MessageHandlerData(theEnv)->CurrentCore->hnd->trace)
        WatchHandler(theEnv,WTRACE,MessageHandlerData(theEnv)->CurrentCore,END_TRACE);
#endif
      ProcedureFunctionData(theEnv)->ReturnFlag = false;
      if ((MessageHandlerData(theEnv)->NextInCore == NULL) || EvaluationData(theEnv)->HaltExecution)
        {
         MessageHandlerData(theEnv)->NextInCore = oldNext;
         MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
         return;
        }
     }

   MessageHandlerData(theEnv)->NextInCore = oldNext;
   MessageHandlerData(theEnv)->CurrentCore = oldCurrent;
  }


/********************************************************
  NAME         : EarlySlotBindError
  DESCRIPTION  : Prints out an error message when
                 a message-handler from a superclass
                 which contains a static-bind
                 slot access is not valid for the
                 currently active instance (i.e.
                 the instance is not using the
                 superclass's slot)
  INPUTS       : 1) The currently active instance
                 2) The defclass holding the invalid slot
                 3) The canonical id of the slot
  RETURNS      : Nothing useful
  SIDE EFFECTS : Error message printed
  NOTES        : None
 ********************************************************/
static void EarlySlotBindError(
  Environment *theEnv,
  Instance *theInstance,
  Defclass *theDefclass,
  unsigned slotID)
  {
   SlotDescriptor *sd;

   sd = theDefclass->instanceTemplate[theDefclass->slotNameMap[slotID] - 1];
   PrintErrorID(theEnv,"MSGPASS",3,false);
   EnvPrintRouter(theEnv,WERROR,"Static reference to slot ");
   EnvPrintRouter(theEnv,WERROR,ValueToString(sd->slotName->name));
   EnvPrintRouter(theEnv,WERROR," of class ");
   PrintClassName(theEnv,WERROR,theDefclass,false);
   EnvPrintRouter(theEnv,WERROR," does not apply to ");
   PrintInstanceNameAndClass(theEnv,WERROR,theInstance,true);
  }

#endif /* OBJECT_SYSTEM */

