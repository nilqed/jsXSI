   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*            DEFTEMPLATE UTILITIES MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides utility routines for deftemplates.      */
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
/*            Added additional arguments to                  */
/*            InvalidDeftemplateSlotMessage function.        */
/*                                                           */
/*            Added additional arguments to                  */
/*            PrintTemplateFact function.                    */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Used gensprintf instead of sprintf.            */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Added Env prefix to GetEvaluationError and     */
/*            SetEvaluationError functions.                  */
/*                                                           */
/*            Added Env prefix to GetHaltExecution and       */
/*            SetHaltExecution functions.                    */
/*                                                           */
/*            Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*      6.50: Watch facts for modify command only prints     */
/*            changed slots.                                 */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if DEFTEMPLATE_CONSTRUCT

#include <stdio.h>
#include <string.h>

#include "argacces.h"
#include "constrct.h"
#include "cstrnchk.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "modulutl.h"
#include "router.h"
#include "sysdep.h"
#include "tmpltbsc.h"
#include "tmpltdef.h"
#include "tmpltfun.h"
#include "tmpltpsr.h"
#include "watch.h"

#include "tmpltutl.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                     PrintTemplateSlot(Environment *,const char *,struct templateSlot *,struct field *);
   static struct templateSlot     *GetNextTemplateSlotToPrint(void *,struct fact *,struct templateSlot *,int *,int,const char *);

/********************************************************/
/* InvalidDeftemplateSlotMessage: Generic error message */
/*   for use when a specified slot name isn't defined   */
/*   in its corresponding deftemplate.                  */
/********************************************************/
void InvalidDeftemplateSlotMessage(
  Environment *theEnv,
  const char *slotName,
  const char *deftemplateName,
  bool printCR)
  {
   PrintErrorID(theEnv,"TMPLTDEF",1,printCR);
   EnvPrintRouter(theEnv,WERROR,"Invalid slot ");
   EnvPrintRouter(theEnv,WERROR,slotName);
   EnvPrintRouter(theEnv,WERROR," not defined in corresponding deftemplate ");
   EnvPrintRouter(theEnv,WERROR,deftemplateName);
   EnvPrintRouter(theEnv,WERROR,".\n");
  }

/**********************************************************/
/* SingleFieldSlotCardinalityError: Generic error message */
/*   used when an attempt is made to placed a multifield  */
/*   value into a single field slot.                      */
/**********************************************************/
void SingleFieldSlotCardinalityError(
  Environment *theEnv,
  const char *slotName)
  {
   PrintErrorID(theEnv,"TMPLTDEF",2,true);
   EnvPrintRouter(theEnv,WERROR,"The single field slot ");
   EnvPrintRouter(theEnv,WERROR,slotName);
   EnvPrintRouter(theEnv,WERROR," can only contain a single field value.\n");
  }

/**********************************************************************/
/* MultiIntoSingleFieldSlotError: Determines if a multifield value is */
/*   being placed into a single field slot of a deftemplate fact.     */
/**********************************************************************/
void MultiIntoSingleFieldSlotError(
  Environment *theEnv,
  struct templateSlot *theSlot,
  Deftemplate *theDeftemplate)
  {
   PrintErrorID(theEnv,"TMPLTFUN",2,true);
   EnvPrintRouter(theEnv,WERROR,"Attempted to assert a multifield value \n");
   EnvPrintRouter(theEnv,WERROR,"into the single field slot ");
   if (theSlot != NULL) EnvPrintRouter(theEnv,WERROR,theSlot->slotName->contents);
   else EnvPrintRouter(theEnv,WERROR,"<<unknown>>");
   EnvPrintRouter(theEnv,WERROR," of deftemplate ");
   if (theDeftemplate != NULL) EnvPrintRouter(theEnv,WERROR,theDeftemplate->header.name->contents);
   else EnvPrintRouter(theEnv,WERROR,"<<unknown>>");
   EnvPrintRouter(theEnv,WERROR,".\n");

   EnvSetEvaluationError(theEnv,true);
  }

/**************************************************************/
/* CheckTemplateFact: Checks a fact to see if it violates any */
/*   deftemplate type, allowed-..., or range specifications.  */
/**************************************************************/
void CheckTemplateFact(
  Environment *theEnv,
  Fact *theFact)
  {
   struct field *sublist;
   int i;
   Deftemplate *theDeftemplate;
   struct templateSlot *slotPtr;
   CLIPSValue theData;
   char thePlace[20];
   int rv;

   if (! EnvGetDynamicConstraintChecking(theEnv)) return;

   sublist = theFact->theProposition.theFields;

   /*========================================================*/
   /* If the deftemplate corresponding to the first field of */
   /* of the fact cannot be found, then the fact cannot be   */
   /* checked against the deftemplate format.                */
   /*========================================================*/

   theDeftemplate = theFact->whichDeftemplate;
   if (theDeftemplate == NULL) return;
   if (theDeftemplate->implied) return;

   /*=============================================*/
   /* Check each of the slots of the deftemplate. */
   /*=============================================*/

   i = 0;
   for (slotPtr = theDeftemplate->slotList;
        slotPtr != NULL;
        slotPtr = slotPtr->next)
     {
      /*================================================*/
      /* Store the slot value in the appropriate format */
      /* for a call to the constraint checking routine. */
      /*================================================*/

      if (slotPtr->multislot == false)
        {
         theData.type = sublist[i].type;
         theData.value = sublist[i].value;
         i++;
        }
      else
        {
         theData.type = MULTIFIELD;
         theData.value = (void *) sublist[i].value;
         SetDOBegin(theData,1);
         SetDOEnd(theData,((struct multifield *) sublist[i].value)->multifieldLength);
         i++;
        }

      /*=============================================*/
      /* Call the constraint checking routine to see */
      /* if a constraint violation occurred.         */
      /*=============================================*/

      rv = ConstraintCheckDataObject(theEnv,&theData,slotPtr->constraints);
      if (rv != NO_VIOLATION)
        {
         gensprintf(thePlace,"fact f-%-5lld ",theFact->factIndex);

         PrintErrorID(theEnv,"CSTRNCHK",1,true);
         EnvPrintRouter(theEnv,WERROR,"Slot value ");
         PrintDataObject(theEnv,WERROR,&theData);
         EnvPrintRouter(theEnv,WERROR," ");
         ConstraintViolationErrorMessage(theEnv,NULL,thePlace,false,0,slotPtr->slotName,
                                         0,rv,slotPtr->constraints,true);
         EnvSetHaltExecution(theEnv,true);
         return;
        }
     }

   return;
  }

/***********************************************************************/
/* CheckRHSSlotTypes: Checks the validity of a change to a slot as the */
/*   result of an assert, modify, or duplicate command. This checking  */
/*   is performed statically (i.e. when the command is being parsed).  */
/***********************************************************************/
bool CheckRHSSlotTypes(
  Environment *theEnv,
  struct expr *rhsSlots,
  struct templateSlot *slotPtr,
  const char *thePlace)
  {
   int rv;
   const char *theName;

   rv = ConstraintCheckExpressionChain(theEnv,rhsSlots,slotPtr->constraints);
   if (rv != NO_VIOLATION)
     {
      if (rv != CARDINALITY_VIOLATION) theName = "A literal slot value";
      else theName = "Literal slot values";
      ConstraintViolationErrorMessage(theEnv,theName,thePlace,true,0,
                                      slotPtr->slotName,0,rv,slotPtr->constraints,true);
      return false;
     }

   return true;
  }

/*********************************************************/
/* GetNthSlot: Given a deftemplate and an integer index, */
/*   returns the nth slot of a deftemplate.              */
/*********************************************************/
struct templateSlot *GetNthSlot(
  Deftemplate *theDeftemplate,
  int position)
  {
   struct templateSlot *slotPtr;
   int i = 0;

   slotPtr = theDeftemplate->slotList;
   while (slotPtr != NULL)
     {
      if (i == position) return(slotPtr);
      slotPtr = slotPtr->next;
      i++;
     }

   return NULL;
  }

/*******************************************************/
/* FindSlotPosition: Finds the position of a specified */
/*   slot in a deftemplate structure.                  */
/*******************************************************/
int FindSlotPosition(
  Deftemplate *theDeftemplate,
  SYMBOL_HN *name)
  {
   struct templateSlot *slotPtr;
   int position;

   for (slotPtr = theDeftemplate->slotList, position = 1;
        slotPtr != NULL;
        slotPtr = slotPtr->next, position++)
     {
      if (slotPtr->slotName == name)
        { return(position); }
     }

   return 0;
  }

/**********************/
/* PrintTemplateSlot: */
/**********************/
static void PrintTemplateSlot(
  Environment *theEnv,
  const char *logicalName,
  struct templateSlot *slotPtr,
  struct field *slotValue)
  {
   EnvPrintRouter(theEnv,logicalName,"(");
   EnvPrintRouter(theEnv,logicalName,slotPtr->slotName->contents);

   /*======================================================*/
   /* Print the value of the slot for a single field slot. */
   /*======================================================*/

   if (slotPtr->multislot == false)
     {
      EnvPrintRouter(theEnv,logicalName," ");
      PrintAtom(theEnv,logicalName,slotValue->type,slotValue->value);
     }

   /*==========================================================*/
   /* Else print the value of the slot for a multi field slot. */
   /*==========================================================*/

   else
     {
      struct multifield *theSegment;

      theSegment = (struct multifield *) slotValue->value;
      if (theSegment->multifieldLength > 0)
        {
         EnvPrintRouter(theEnv,logicalName," ");
         PrintMultifield(theEnv,logicalName,theSegment,
                         0,(long) theSegment->multifieldLength-1,false);
        }
     }

   /*============================================*/
   /* Print the closing parenthesis of the slot. */
   /*============================================*/

   EnvPrintRouter(theEnv,logicalName,")");
  }

/********************************/
/* GetNextTemplateSloteToPrint: */
/********************************/
static struct templateSlot *GetNextTemplateSlotToPrint(
  void *theEnv,
  struct fact *theFact,
  struct templateSlot *slotPtr,
  int *position,
  int ignoreDefaults,
  const char *changeMap)
  {
   CLIPSValue tempDO;
   struct field *sublist;

   sublist = theFact->theProposition.theFields;
   if (slotPtr == NULL)
     { slotPtr = theFact->whichDeftemplate->slotList; }
   else
     {
      slotPtr = slotPtr->next;
      (*position)++;
     }
     
   while (slotPtr != NULL)
     {
      if ((changeMap != NULL) && (TestBitMap(changeMap,*position) == 0))
        {
         (*position)++;
         slotPtr = slotPtr->next;
         continue;
        }

      if (ignoreDefaults && (slotPtr->defaultDynamic == false))
        {
         DeftemplateSlotDefault(theEnv,theFact->whichDeftemplate,slotPtr,&tempDO,true);
         
         if (slotPtr->multislot == false)
           {
            if ((GetType(tempDO) == sublist[*position].type) &&
                (GetValue(tempDO) == sublist[*position].value))
              {     
               (*position)++;
               slotPtr = slotPtr->next;
               continue;
              }
           }
         else if (MultifieldsEqual((struct multifield*) GetValue(tempDO),
                                   (struct multifield *) sublist[*position].value))
           {
            (*position)++;
            slotPtr = slotPtr->next;
            continue;
           }
        }
        
      return slotPtr;
     }
     
   return NULL;
  }

/**********************************************************/
/* PrintTemplateFact: Prints a fact using the deftemplate */
/*   format. Returns true if the fact was printed using   */
/*   this format, otherwise false.                        */
/**********************************************************/
void PrintTemplateFact(
  Environment *theEnv,
  const char *logicalName,
  Fact *theFact,
  bool separateLines,
  bool ignoreDefaults,
  const char *changeMap)
  {
   struct field *sublist;
   int i;
   Deftemplate *theDeftemplate;
   struct templateSlot *slotPtr, *lastPtr = NULL;
   bool slotPrinted = false;
   
   /*==============================*/
   /* Initialize some information. */
   /*==============================*/

   theDeftemplate = theFact->whichDeftemplate;
   sublist = theFact->theProposition.theFields;

   /*=============================================*/
   /* Print the relation name of the deftemplate. */
   /*=============================================*/

   EnvPrintRouter(theEnv,logicalName,"(");
   EnvPrintRouter(theEnv,logicalName,theDeftemplate->header.name->contents);

   /*===================================================*/
   /* Print each of the field slots of the deftemplate. */
   /*===================================================*/

   i = 0;
   slotPtr = GetNextTemplateSlotToPrint(theEnv,theFact,lastPtr,&i,
                                        ignoreDefaults,changeMap);
     
   if ((changeMap != NULL) &&
       (theFact->whichDeftemplate->slotList != slotPtr))
     { EnvPrintRouter(theEnv,logicalName," ..."); }
   
   while (slotPtr != NULL)
     {
      /*===========================================*/
      /* Print the opening parenthesis of the slot */
      /* and the slot name.                        */
      /*===========================================*/
     
      if (! slotPrinted) 
        { 
         slotPrinted = true;
         EnvPrintRouter(theEnv,logicalName," "); 
        }

      if (separateLines)
        { EnvPrintRouter(theEnv,logicalName,"\n   "); }

      /*===================================*/
      /* Print the slot name and it value. */
      /*===================================*/
      
      PrintTemplateSlot(theEnv,logicalName,slotPtr,&sublist[i]);

      /*===========================*/
      /* Move on to the next slot. */
      /*===========================*/
      
      lastPtr = slotPtr;
      slotPtr = GetNextTemplateSlotToPrint(theEnv,theFact,lastPtr,&i,
                                           ignoreDefaults,changeMap);

      if ((changeMap != NULL) && (lastPtr->next != slotPtr))
        { EnvPrintRouter(theEnv,logicalName," ..."); }
      
      if (slotPtr != NULL) EnvPrintRouter(theEnv,logicalName," ");
     }

   EnvPrintRouter(theEnv,logicalName,")");
  }

/***************************************************************************/
/* UpdateDeftemplateScope: Updates the scope flag of all the deftemplates. */
/***************************************************************************/
void UpdateDeftemplateScope(
  Environment *theEnv)
  {
   Deftemplate *theDeftemplate;
   int moduleCount;
   Defmodule *theModule;
   struct defmoduleItemHeader *theItem;

   /*==================================*/
   /* Loop through all of the modules. */
   /*==================================*/

   for (theModule = EnvGetNextDefmodule(theEnv,NULL);
        theModule != NULL;
        theModule = EnvGetNextDefmodule(theEnv,theModule))
     {
      /*======================================================*/
      /* Loop through each of the deftemplates in the module. */
      /*======================================================*/

      theItem = (struct defmoduleItemHeader *)
                GetModuleItem(theEnv,theModule,DeftemplateData(theEnv)->DeftemplateModuleIndex);

      for (theDeftemplate = (Deftemplate *) theItem->firstItem;
           theDeftemplate != NULL ;
           theDeftemplate = EnvGetNextDeftemplate(theEnv,theDeftemplate))
        {
         /*=======================================*/
         /* If the deftemplate can be seen by the */
         /* current module, then it is in scope.  */
         /*=======================================*/

         if (FindImportedConstruct(theEnv,"deftemplate",theModule,
                                   ValueToString(theDeftemplate->header.name),
                                   &moduleCount,true,NULL) != NULL)
           { theDeftemplate->inScope = true; }
         else
           { theDeftemplate->inScope = false; }
        }
     }
  }

/****************************************************************/
/* FindSlot: Finds a specified slot in a deftemplate structure. */
/****************************************************************/
struct templateSlot *FindSlot(
  Deftemplate *theDeftemplate,
  SYMBOL_HN *name,
  short *whichOne)
  {
   struct templateSlot *slotPtr;

   *whichOne = 1;
   slotPtr = theDeftemplate->slotList;
   while (slotPtr != NULL)
     {
      if (slotPtr->slotName == name)
        { return(slotPtr); }
      (*whichOne)++;
      slotPtr = slotPtr->next;
     }

   *whichOne = -1;
   return NULL;
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/************************************************************/
/* CreateImpliedDeftemplate: Creates an implied deftemplate */
/*   and adds it to the list of deftemplates.               */
/************************************************************/
Deftemplate *CreateImpliedDeftemplate(
  Environment *theEnv,
  SYMBOL_HN *deftemplateName,
  bool setFlag)
  {
   Deftemplate *newDeftemplate;

   newDeftemplate = get_struct(theEnv,deftemplate);
   newDeftemplate->header.name = deftemplateName;
   newDeftemplate->header.ppForm = NULL;
   newDeftemplate->header.usrData = NULL;
   newDeftemplate->slotList = NULL;
   newDeftemplate->implied = setFlag;
   newDeftemplate->numberOfSlots = 0;
   newDeftemplate->inScope = 1;
   newDeftemplate->patternNetwork = NULL;
   newDeftemplate->factList = NULL;
   newDeftemplate->lastFact = NULL;
   newDeftemplate->busyCount = 0;
   newDeftemplate->watch = false;
   newDeftemplate->header.next = NULL;

#if DEBUGGING_FUNCTIONS
   if (EnvGetWatchItem(theEnv,"facts"))
     { EnvSetDeftemplateWatch(theEnv,true,newDeftemplate); }
#endif

   newDeftemplate->header.whichModule = (struct defmoduleItemHeader *)
                                        GetModuleItem(theEnv,NULL,DeftemplateData(theEnv)->DeftemplateModuleIndex);

   AddConstructToModule(&newDeftemplate->header);
   InstallDeftemplate(theEnv,newDeftemplate);

   return(newDeftemplate);
  }

#endif

#endif /* DEFTEMPLATE_CONSTRUCT */

