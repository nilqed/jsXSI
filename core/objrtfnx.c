   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*    INFERENCE ENGINE OBJECT ACCESS ROUTINES MODULE   */
   /*******************************************************/

/*************************************************************/
/* Purpose: RETE Network Interface for Objects               */
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
/*      6.24: Converted INSTANCE_PATTERN_MATCHING to         */
/*            DEFRULE_CONSTRUCT.                             */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added support for hashed alpha memories.       */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/
/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFRULE_CONSTRUCT && OBJECT_SYSTEM

#include <stdio.h>
#include <string.h>

#include "classcom.h"
#include "classfun.h"

#if DEVELOPER
#include "exprnops.h"
#endif
#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constant.h"
#include "drive.h"
#include "engine.h"
#include "envrnmnt.h"
#include "memalloc.h"
#include "multifld.h"
#include "objrtmch.h"
#include "reteutil.h"
#include "router.h"

#include "objrtfnx.h"

/* =========================================
   *****************************************
                 MACROS AND TYPES
   =========================================
   ***************************************** */

#define GetInsSlot(ins,si) ins->slotAddresses[ins->cls->slotNameMap[si]-1]

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                    PrintObjectGetVarJN1(Environment *,const char *,void *);
   static bool                    ObjectGetVarJNFunction1(Environment *,void *,CLIPSValue *);
   static void                    PrintObjectGetVarJN2(Environment *,const char *,void *);
   static bool                    ObjectGetVarJNFunction2(Environment *,void *,CLIPSValue *);
   static void                    PrintObjectGetVarPN1(Environment *,const char *,void *);
   static bool                    ObjectGetVarPNFunction1(Environment *,void *,CLIPSValue *);
   static void                    PrintObjectGetVarPN2(Environment *,const char *,void *);
   static bool                    ObjectGetVarPNFunction2(Environment *,void *,CLIPSValue *);
   static void                    PrintObjectCmpConstant(Environment *,const char *,void *);
   static void                    PrintSlotLengthTest(Environment *,const char *,void *);
   static bool                    SlotLengthTestFunction(Environment *,void *,CLIPSValue *);
   static void                    PrintPNSimpleCompareFunction1(Environment *,const char *,void *);
   static bool                    PNSimpleCompareFunction1(Environment *,void *,CLIPSValue *);
   static void                    PrintPNSimpleCompareFunction2(Environment *,const char *,void *);
   static bool                    PNSimpleCompareFunction2(Environment *,void *,CLIPSValue *);
   static void                    PrintPNSimpleCompareFunction3(Environment *,const char *,void *);
   static bool                    PNSimpleCompareFunction3(Environment *,void *,CLIPSValue *);
   static void                    PrintJNSimpleCompareFunction1(Environment *,const char *,void *);
   static bool                    JNSimpleCompareFunction1(Environment *,void *,CLIPSValue *);
   static void                    PrintJNSimpleCompareFunction2(Environment *,const char *,void *);
   static bool                    JNSimpleCompareFunction2(Environment *,void *,CLIPSValue *);
   static void                    PrintJNSimpleCompareFunction3(Environment *,const char *,void *);
   static bool                    JNSimpleCompareFunction3(Environment *,void *,CLIPSValue *);
   static void                    GetPatternObjectAndMarks(Environment *,int,int,int,Instance **,struct multifieldMarker **);
   static void                    GetObjectValueGeneral(Environment *,CLIPSValue *,Instance *,
                                                        struct multifieldMarker *,struct ObjectMatchVar1 *);
   static void                    GetObjectValueSimple(Environment *,CLIPSValue *,Instance *,struct ObjectMatchVar2 *);
   static long                    CalculateSlotField(struct multifieldMarker *,INSTANCE_SLOT *,long,long *); /* 6.04 Bug Fix */
   static void                    GetInsMultiSlotField(FIELD *,Instance *,unsigned,unsigned,unsigned);
   static void                    DeallocateObjectReteData(Environment *);
   static void                    DestroyObjectPatternNetwork(Environment *,OBJECT_PATTERN_NODE *);
   static void                    DestroyObjectAlphaNodes(Environment *,OBJECT_ALPHA_NODE *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : InstallObjectPrimitives
  DESCRIPTION  : Installs all the entity records
                 associated with object pattern
                 matching operations
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Primitive operations installed
  NOTES        : None
 ***************************************************/
void InstallObjectPrimitives(
  Environment *theEnv)
  {
   struct entityRecord objectGVInfo1 = { "OBJ_GET_SLOT_JNVAR1", OBJ_GET_SLOT_JNVAR1,0,1,0,
                                             PrintObjectGetVarJN1,
                                             PrintObjectGetVarJN1,NULL,
                                             ObjectGetVarJNFunction1,
                                             NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord objectGVInfo2 = { "OBJ_GET_SLOT_JNVAR2", OBJ_GET_SLOT_JNVAR2,0,1,0,
                                             PrintObjectGetVarJN2,
                                             PrintObjectGetVarJN2,NULL,
                                             ObjectGetVarJNFunction2,
                                             NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord objectGVPNInfo1 = { "OBJ_GET_SLOT_PNVAR1", OBJ_GET_SLOT_PNVAR1,0,1,0,
                                               PrintObjectGetVarPN1,
                                               PrintObjectGetVarPN1,NULL,
                                               ObjectGetVarPNFunction1,
                                               NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord objectGVPNInfo2 = { "OBJ_GET_SLOT_PNVAR2", OBJ_GET_SLOT_PNVAR2,0,1,0,
                                               PrintObjectGetVarPN2,
                                               PrintObjectGetVarPN2,NULL,
                                               ObjectGetVarPNFunction2,
                                               NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord objectCmpConstantInfo = { "OBJ_PN_CONSTANT", OBJ_PN_CONSTANT,0,1,1,
                                                     PrintObjectCmpConstant,
                                                     PrintObjectCmpConstant,NULL,
                                                     ObjectCmpConstantFunction,
                                                     NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord lengthTestInfo = { "OBJ_SLOT_LENGTH", OBJ_SLOT_LENGTH,0,1,0,
                                              PrintSlotLengthTest,
                                              PrintSlotLengthTest,NULL,
                                              SlotLengthTestFunction,
                                              NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord pNSimpleCompareInfo1 = { "OBJ_PN_CMP1", OBJ_PN_CMP1,0,1,1,
                                                    PrintPNSimpleCompareFunction1,
                                                    PrintPNSimpleCompareFunction1,NULL,
                                                    PNSimpleCompareFunction1,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord pNSimpleCompareInfo2 = { "OBJ_PN_CMP2", OBJ_PN_CMP2,0,1,1,
                                                    PrintPNSimpleCompareFunction2,
                                                    PrintPNSimpleCompareFunction2,NULL,
                                                    PNSimpleCompareFunction2,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord pNSimpleCompareInfo3 = { "OBJ_PN_CMP3", OBJ_PN_CMP3,0,1,1,
                                                    PrintPNSimpleCompareFunction3,
                                                    PrintPNSimpleCompareFunction3,NULL,
                                                    PNSimpleCompareFunction3,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord jNSimpleCompareInfo1 = { "OBJ_JN_CMP1", OBJ_JN_CMP1,0,1,1,
                                                    PrintJNSimpleCompareFunction1,
                                                    PrintJNSimpleCompareFunction1,NULL,
                                                    JNSimpleCompareFunction1,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord jNSimpleCompareInfo2 = { "OBJ_JN_CMP2", OBJ_JN_CMP2,0,1,1,
                                                    PrintJNSimpleCompareFunction2,
                                                    PrintJNSimpleCompareFunction2,NULL,
                                                    JNSimpleCompareFunction2,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   struct entityRecord jNSimpleCompareInfo3 = { "OBJ_JN_CMP3", OBJ_JN_CMP3,0,1,1,
                                                    PrintJNSimpleCompareFunction3,
                                                    PrintJNSimpleCompareFunction3,NULL,
                                                    JNSimpleCompareFunction3,
                                                    NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

   AllocateEnvironmentData(theEnv,OBJECT_RETE_DATA,sizeof(struct objectReteData),DeallocateObjectReteData);
   ObjectReteData(theEnv)->CurrentObjectSlotLength = 1;

   memcpy(&ObjectReteData(theEnv)->ObjectGVInfo1,&objectGVInfo1,sizeof(struct entityRecord));  
   memcpy(&ObjectReteData(theEnv)->ObjectGVInfo2,&objectGVInfo2,sizeof(struct entityRecord));
   memcpy(&ObjectReteData(theEnv)->ObjectGVPNInfo1,&objectGVPNInfo1,sizeof(struct entityRecord));
   memcpy(&ObjectReteData(theEnv)->ObjectGVPNInfo2,&objectGVPNInfo2,sizeof(struct entityRecord));
   memcpy(&ObjectReteData(theEnv)->ObjectCmpConstantInfo,&objectCmpConstantInfo,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->LengthTestInfo,&lengthTestInfo,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->PNSimpleCompareInfo1,&pNSimpleCompareInfo1,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->PNSimpleCompareInfo2,&pNSimpleCompareInfo2,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->PNSimpleCompareInfo3,&pNSimpleCompareInfo3,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->JNSimpleCompareInfo1,&jNSimpleCompareInfo1,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->JNSimpleCompareInfo2,&jNSimpleCompareInfo2,sizeof(struct entityRecord)); 
   memcpy(&ObjectReteData(theEnv)->JNSimpleCompareInfo3,&jNSimpleCompareInfo3,sizeof(struct entityRecord)); 

   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->ObjectGVInfo1,OBJ_GET_SLOT_JNVAR1);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->ObjectGVInfo2,OBJ_GET_SLOT_JNVAR2);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->ObjectGVPNInfo1,OBJ_GET_SLOT_PNVAR1);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->ObjectGVPNInfo2,OBJ_GET_SLOT_PNVAR2);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->ObjectCmpConstantInfo,OBJ_PN_CONSTANT);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->LengthTestInfo,OBJ_SLOT_LENGTH);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->PNSimpleCompareInfo1,OBJ_PN_CMP1);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->PNSimpleCompareInfo2,OBJ_PN_CMP2);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->PNSimpleCompareInfo3,OBJ_PN_CMP3);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->JNSimpleCompareInfo1,OBJ_JN_CMP1);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->JNSimpleCompareInfo2,OBJ_JN_CMP2);
   InstallPrimitive(theEnv,&ObjectReteData(theEnv)->JNSimpleCompareInfo3,OBJ_JN_CMP3);
  }
  
/*****************************************************/
/* DeallocateObjectReteData: Deallocates environment */
/*    data for the object rete network.              */
/*****************************************************/
static void DeallocateObjectReteData(
  Environment *theEnv)
  {
   OBJECT_PATTERN_NODE *theNetwork;
   
#if BLOAD || BLOAD_AND_BSAVE
   if (Bloaded(theEnv)) return;
#endif
   
   theNetwork = ObjectReteData(theEnv)->ObjectPatternNetworkPointer;
   DestroyObjectPatternNetwork(theEnv,theNetwork);
  }
  
/****************************************************************/
/* DestroyObjectPatternNetwork: Deallocates the data structures */
/*   associated with the object pattern network.                */
/****************************************************************/
static void DestroyObjectPatternNetwork(
  Environment *theEnv,
  OBJECT_PATTERN_NODE *thePattern)
  {
   OBJECT_PATTERN_NODE *patternPtr;
   
   if (thePattern == NULL) return;
   
   while (thePattern != NULL)
     {
      patternPtr = thePattern->rightNode;
      
      DestroyObjectPatternNetwork(theEnv,thePattern->nextLevel);
      DestroyObjectAlphaNodes(theEnv,thePattern->alphaNode);
#if ! RUN_TIME      
      rtn_struct(theEnv,objectPatternNode,thePattern);
#endif      
      thePattern = patternPtr;
     }
  }
  
/************************************************************/
/* DestroyObjectAlphaNodes: Deallocates the data structures */
/*   associated with the object alpha nodes.                */
/************************************************************/
static void DestroyObjectAlphaNodes(
  Environment *theEnv,
  OBJECT_ALPHA_NODE *theNode)
  {
   OBJECT_ALPHA_NODE *nodePtr;
   
   if (theNode == NULL) return;
   
   while (theNode != NULL)
     {
      nodePtr = theNode->nxtInGroup;
       
      DestroyAlphaMemory(theEnv,&theNode->header,false);

#if ! RUN_TIME
      rtn_struct(theEnv,objectAlphaNode,theNode);
#endif
      
      theNode = nodePtr;
     }
  }

/*****************************************************
  NAME         : ObjectCmpConstantFunction
  DESCRIPTION  : Used to compare object slot values
                 against a constant
  INPUTS       : 1) The constant test bitmap
                 2) Data object buffer to hold result
  RETURNS      : True if test successful,
                 false otherwise
  SIDE EFFECTS : Buffer set to symbol TRUE if test
                 successful, false otherwise
  NOTES        : Called directly by
                   EvaluatePatternExpression()
 *****************************************************/
bool ObjectCmpConstantFunction(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectCmpPNConstant *hack;
   CLIPSValue theVar;
   EXPRESSION *constantExp;
   int rv;
   SEGMENT *theSegment;

   hack = (struct ObjectCmpPNConstant *) ValueToBitMap(theValue);
   if (hack->general)
     {
      EvaluateExpression(theEnv,GetFirstArgument(),&theVar);
      constantExp = GetFirstArgument()->nextArg;
     }
   else
     {
      constantExp = GetFirstArgument();
      if (ObjectReteData(theEnv)->CurrentPatternObjectSlot->type == MULTIFIELD)
        {
         theSegment = (struct multifield *) ObjectReteData(theEnv)->CurrentPatternObjectSlot->value;
         if (hack->fromBeginning)
           {
            theVar.type = theSegment->theFields[hack->offset].type;
            theVar.value = theSegment->theFields[hack->offset].value;
           }
         else
           {
            theVar.type = theSegment->theFields[theSegment->multifieldLength -
                                      (hack->offset + 1)].type;
            theVar.value = theSegment->theFields[theSegment->multifieldLength -
                                      (hack->offset + 1)].value;
           }
        }
      else
        {
         theVar.type = (unsigned short) ObjectReteData(theEnv)->CurrentPatternObjectSlot->type;
         theVar.value = ObjectReteData(theEnv)->CurrentPatternObjectSlot->value;
        }
     }
   if (theVar.type != constantExp->type)
     rv = hack->fail;
   else if (theVar.value != constantExp->value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

static void PrintObjectGetVarJN1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectMatchVar1 *hack;

   hack = (struct ObjectMatchVar1 *) ValueToBitMap(theValue);

   if (hack->objectAddress)
     {
      EnvPrintRouter(theEnv,logicalName,"(obj-ptr ");
      PrintLongInteger(theEnv,logicalName,(long long) hack->whichPattern);
     }
   else if (hack->allFields)
     {
      EnvPrintRouter(theEnv,logicalName,"(obj-slot-contents ");
      PrintLongInteger(theEnv,logicalName,(long long) hack->whichPattern);
      EnvPrintRouter(theEnv,logicalName," ");
      EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
     }
   else
     {
      EnvPrintRouter(theEnv,logicalName,"(obj-slot-var ");
      PrintLongInteger(theEnv,logicalName,(long long) hack->whichPattern);
      EnvPrintRouter(theEnv,logicalName," ");
      EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
      EnvPrintRouter(theEnv,logicalName," ");
      PrintLongInteger(theEnv,logicalName,(long long) hack->whichField);
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

static bool ObjectGetVarJNFunction1(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectMatchVar1 *hack;
   Instance *theInstance;
   struct multifieldMarker *theMarks;
   
   hack = (struct ObjectMatchVar1 *) ValueToBitMap(theValue);
   GetPatternObjectAndMarks(theEnv,((int) hack->whichPattern),hack->lhs,hack->rhs,&theInstance,&theMarks);
   GetObjectValueGeneral(theEnv,theResult,theInstance,theMarks,hack);
   return true;
  }

static void PrintObjectGetVarJN2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectMatchVar2 *hack;

   hack = (struct ObjectMatchVar2 *) ValueToBitMap(theValue);
   EnvPrintRouter(theEnv,logicalName,"(obj-slot-quick-var ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->whichPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
   if (hack->fromBeginning)
     {
      EnvPrintRouter(theEnv,logicalName," B");
      PrintLongInteger(theEnv,logicalName,(long long) (hack->beginningOffset + 1));
     }
   if (hack->fromEnd)
     {
      EnvPrintRouter(theEnv,logicalName," E");
      PrintLongInteger(theEnv,logicalName,(long long) (hack->endOffset + 1));
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

static bool ObjectGetVarJNFunction2(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectMatchVar2 *hack;
   Instance *theInstance;
   struct multifieldMarker *theMarks;
   
   hack = (struct ObjectMatchVar2 *) ValueToBitMap(theValue);
   GetPatternObjectAndMarks(theEnv,((int) hack->whichPattern),hack->lhs,hack->rhs,&theInstance,&theMarks);
   GetObjectValueSimple(theEnv,theResult,theInstance,hack);
   return true;
  }

static void PrintObjectGetVarPN1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectMatchVar1 *hack;

   hack = (struct ObjectMatchVar1 *) ValueToBitMap(theValue);

   if (hack->objectAddress)
     EnvPrintRouter(theEnv,logicalName,"(ptn-obj-ptr ");
   else if (hack->allFields)
     {
      EnvPrintRouter(theEnv,logicalName,"(ptn-obj-slot-contents ");
      EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
     }
   else
     {
      EnvPrintRouter(theEnv,logicalName,"(ptn-obj-slot-var ");
      EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
      EnvPrintRouter(theEnv,logicalName," ");
      PrintLongInteger(theEnv,logicalName,(long long) hack->whichField);
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

static bool ObjectGetVarPNFunction1(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectMatchVar1 *hack;

   hack = (struct ObjectMatchVar1 *) ValueToBitMap(theValue);
   GetObjectValueGeneral(theEnv,theResult,ObjectReteData(theEnv)->CurrentPatternObject,ObjectReteData(theEnv)->CurrentPatternObjectMarks,hack);
   return true;
  }

static void PrintObjectGetVarPN2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectMatchVar2 *hack;

   hack = (struct ObjectMatchVar2 *) ValueToBitMap(theValue);
   EnvPrintRouter(theEnv,logicalName,"(ptn-obj-slot-quick-var ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->whichSlot)));
   if (hack->fromBeginning)
     {
      EnvPrintRouter(theEnv,logicalName," B");
      PrintLongInteger(theEnv,logicalName,(long long) (hack->beginningOffset + 1));
     }
   if (hack->fromEnd)
     {
      EnvPrintRouter(theEnv,logicalName," E");
      PrintLongInteger(theEnv,logicalName,(long long) (hack->endOffset + 1));
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

static bool ObjectGetVarPNFunction2(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectMatchVar2 *hack;

   hack = (struct ObjectMatchVar2 *) ValueToBitMap(theValue);
   GetObjectValueSimple(theEnv,theResult,ObjectReteData(theEnv)->CurrentPatternObject,hack);
   return true;
  }

static void PrintObjectCmpConstant(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpPNConstant *hack;

   hack = (struct ObjectCmpPNConstant *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(obj-const ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   if (hack->general)
     PrintExpression(theEnv,logicalName,GetFirstArgument());
   else
     {
      EnvPrintRouter(theEnv,logicalName,hack->fromBeginning ? "B" : "E");
      PrintLongInteger(theEnv,logicalName,(long long) hack->offset);
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

static void PrintSlotLengthTest(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectMatchLength *hack;

   hack = (struct ObjectMatchLength *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(obj-slot-len ");
   if (hack->exactly)
     EnvPrintRouter(theEnv,logicalName,"= ");
   else
     EnvPrintRouter(theEnv,logicalName,">= ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->minLength);
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool SlotLengthTestFunction(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectMatchLength *hack;

   theResult->type = SYMBOL;
   theResult->value = EnvFalseSymbol(theEnv);
   hack = (struct ObjectMatchLength *) ValueToBitMap(theValue);
   if (ObjectReteData(theEnv)->CurrentObjectSlotLength < hack->minLength)
     return false;
   if (hack->exactly && (ObjectReteData(theEnv)->CurrentObjectSlotLength > hack->minLength))
     return false;
   theResult->value = EnvTrueSymbol(theEnv);
   return true;
  }

static void PrintPNSimpleCompareFunction1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpPNSingleSlotVars1 *hack;

   hack = (struct ObjectCmpPNSingleSlotVars1 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(pslot-cmp1 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool PNSimpleCompareFunction1(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectCmpPNSingleSlotVars1 *hack;
   INSTANCE_SLOT *is1,*is2;
   int rv;

   hack = (struct ObjectCmpPNSingleSlotVars1 *) ValueToBitMap(theValue);
   is1 = GetInsSlot(ObjectReteData(theEnv)->CurrentPatternObject,hack->firstSlot);
   is2 = GetInsSlot(ObjectReteData(theEnv)->CurrentPatternObject,hack->secondSlot);
   if (is1->type != is2->type)
     rv = hack->fail;
   else if (is1->value != is2->value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

static void PrintPNSimpleCompareFunction2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpPNSingleSlotVars2 *hack;

   hack = (struct ObjectCmpPNSingleSlotVars2 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(pslot-cmp2 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->fromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->offset);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool PNSimpleCompareFunction2(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectCmpPNSingleSlotVars2 *hack;
   int rv;
   FIELD f1;
   INSTANCE_SLOT *is2;

   hack = (struct ObjectCmpPNSingleSlotVars2 *) ValueToBitMap(theValue);
   GetInsMultiSlotField(&f1,ObjectReteData(theEnv)->CurrentPatternObject,(unsigned) hack->firstSlot,
                             (unsigned) hack->fromBeginning,(unsigned) hack->offset);
   is2 = GetInsSlot(ObjectReteData(theEnv)->CurrentPatternObject,hack->secondSlot);
   if (f1.type != is2->type)
     rv = hack->fail;
   else if (f1.value != is2->value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

static void PrintPNSimpleCompareFunction3(
  Environment*theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpPNSingleSlotVars3 *hack;

   hack = (struct ObjectCmpPNSingleSlotVars3 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(pslot-cmp3 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->firstFromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->firstOffset);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->secondFromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->secondOffset);
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool PNSimpleCompareFunction3(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   struct ObjectCmpPNSingleSlotVars3 *hack;
   int rv;
   FIELD f1,f2;

   hack = (struct ObjectCmpPNSingleSlotVars3 *) ValueToBitMap(theValue);
   GetInsMultiSlotField(&f1,ObjectReteData(theEnv)->CurrentPatternObject,(unsigned) hack->firstSlot,
                        (unsigned) hack->firstFromBeginning,(unsigned) hack->firstOffset);
   GetInsMultiSlotField(&f2,ObjectReteData(theEnv)->CurrentPatternObject,(unsigned) hack->secondSlot,
                        (unsigned) hack->secondFromBeginning,(unsigned) hack->secondOffset);
   if (f1.type != f2.type)
     rv = hack->fail;
   else if (f1.value != f2.value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

static void PrintJNSimpleCompareFunction1(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpJoinSingleSlotVars1 *hack;

   hack = (struct ObjectCmpJoinSingleSlotVars1 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(jslot-cmp1 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->firstPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName," ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->secondPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool JNSimpleCompareFunction1(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   Instance *ins1,*ins2;
   struct multifieldMarker *theMarks;
   struct ObjectCmpJoinSingleSlotVars1 *hack;
   int rv;
   INSTANCE_SLOT *is1,*is2;

   hack = (struct ObjectCmpJoinSingleSlotVars1 *) ValueToBitMap(theValue);
   GetPatternObjectAndMarks(theEnv,((int) hack->firstPattern),hack->firstPatternLHS,hack->firstPatternRHS,&ins1,&theMarks);
   is1 = GetInsSlot(ins1,hack->firstSlot);
   GetPatternObjectAndMarks(theEnv,((int) hack->secondPattern),hack->secondPatternLHS,hack->secondPatternRHS,&ins2,&theMarks);
   is2 = GetInsSlot(ins2,hack->secondSlot);
   if (is1->type != is2->type)
     rv = hack->fail;
   else if (is1->value != is2->value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

static void PrintJNSimpleCompareFunction2(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpJoinSingleSlotVars2 *hack;

   hack = (struct ObjectCmpJoinSingleSlotVars2 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(jslot-cmp2 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->firstPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->fromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->offset);
   EnvPrintRouter(theEnv,logicalName," ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->secondPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool JNSimpleCompareFunction2(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   Instance *ins1,*ins2;
   struct multifieldMarker *theMarks;
   struct ObjectCmpJoinSingleSlotVars2 *hack;
   int rv;
   FIELD f1;
   INSTANCE_SLOT *is2;

   hack = (struct ObjectCmpJoinSingleSlotVars2 *) ValueToBitMap(theValue);
   GetPatternObjectAndMarks(theEnv,((int) hack->firstPattern),hack->firstPatternLHS,hack->firstPatternRHS,&ins1,&theMarks);
   GetInsMultiSlotField(&f1,ins1,(unsigned) hack->firstSlot,
                        (unsigned) hack->fromBeginning,(unsigned) hack->offset);
   GetPatternObjectAndMarks(theEnv,((int) hack->secondPattern),hack->secondPatternLHS,hack->secondPatternRHS,&ins2,&theMarks);
   is2 = GetInsSlot(ins2,hack->secondSlot);
   if (f1.type != is2->type)
     rv = hack->fail;
   else if (f1.value != is2->value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

static void PrintJNSimpleCompareFunction3(
  Environment *theEnv,
  const char *logicalName,
  void *theValue)
  {
#if DEVELOPER
   struct ObjectCmpJoinSingleSlotVars3 *hack;

   hack = (struct ObjectCmpJoinSingleSlotVars3 *) ValueToBitMap(theValue);

   EnvPrintRouter(theEnv,logicalName,"(jslot-cmp3 ");
   EnvPrintRouter(theEnv,logicalName,hack->pass ? "p " : "n ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->firstPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->firstSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->firstFromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->firstOffset);
   EnvPrintRouter(theEnv,logicalName," ");
   PrintLongInteger(theEnv,logicalName,(long long) hack->secondPattern);
   EnvPrintRouter(theEnv,logicalName," ");
   EnvPrintRouter(theEnv,logicalName,ValueToString(FindIDSlotName(theEnv,(unsigned) hack->secondSlot)));
   EnvPrintRouter(theEnv,logicalName,hack->secondFromBeginning ? " B" : " E");
   PrintLongInteger(theEnv,logicalName,(long long) hack->secondOffset);
   EnvPrintRouter(theEnv,logicalName,")");
#else
#if MAC_XCD
#pragma unused(theEnv)
#pragma unused(logicalName)
#pragma unused(theValue)
#endif
#endif
  }

static bool JNSimpleCompareFunction3(
  Environment *theEnv,
  void *theValue,
  CLIPSValue *theResult)
  {
   Instance *ins1,*ins2;
   struct multifieldMarker *theMarks;
   struct ObjectCmpJoinSingleSlotVars3 *hack;
   int rv;
   FIELD f1,f2;

   hack = (struct ObjectCmpJoinSingleSlotVars3 *) ValueToBitMap(theValue);
   GetPatternObjectAndMarks(theEnv,((int) hack->firstPattern),hack->firstPatternLHS,hack->firstPatternRHS,&ins1,&theMarks);
   GetInsMultiSlotField(&f1,ins1,(unsigned) hack->firstSlot,
                        (unsigned) hack->firstFromBeginning,
                        (unsigned) hack->firstOffset);
   GetPatternObjectAndMarks(theEnv,((int) hack->secondPattern),hack->secondPatternLHS,hack->secondPatternRHS,&ins2,&theMarks);
   GetInsMultiSlotField(&f2,ins2,(unsigned) hack->secondSlot,
                        (unsigned) hack->secondFromBeginning,
                        (unsigned) hack->secondOffset);
   if (f1.type != f2.type)
     rv = hack->fail;
   else if (f1.value != f2.value)
     rv = hack->fail;
   else
     rv = hack->pass;
   theResult->type = SYMBOL;
   theResult->value = rv ? EnvTrueSymbol(theEnv) : EnvFalseSymbol(theEnv);
   return(rv);
  }

/****************************************************
  NAME         : GetPatternObjectAndMarks
  DESCRIPTION  : Finds the instance and multfiield
                 markers corresponding to a specified
                 pattern in the join network
  INPUTS       : 1) The index of the desired pattern
                 2) A buffer to hold the instance
                    address
                 3) A buffer to hold the list of
                    multifield markers
  RETURNS      : Nothing useful
  SIDE EFFECTS : Buffers set
  NOTES        : None
 ****************************************************/
static void GetPatternObjectAndMarks(
  Environment *theEnv,
  int pattern,
  int lhs,
  int rhs,
  Instance **theInstance,
  struct multifieldMarker **theMarkers)
  {
   if (lhs)
     {
      *theInstance = (Instance *)
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->matchingItem;
      *theMarkers =
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->markers;
     }
   else if (rhs)
     {
      *theInstance = (Instance *)
        get_nth_pm_match(EngineData(theEnv)->GlobalRHSBinds,pattern)->matchingItem;
      *theMarkers =
        get_nth_pm_match(EngineData(theEnv)->GlobalRHSBinds,pattern)->markers;
     }
   else if (EngineData(theEnv)->GlobalRHSBinds == NULL)
     {
      *theInstance = (Instance *)
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->matchingItem;
      *theMarkers =
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->markers;
     }
   else if ((((int) EngineData(theEnv)->GlobalJoin->depth) - 1) == pattern)
     {
      *theInstance = (Instance *) 
        get_nth_pm_match(EngineData(theEnv)->GlobalRHSBinds,0)->matchingItem;
      *theMarkers = get_nth_pm_match(EngineData(theEnv)->GlobalRHSBinds,0)->markers;
     }
   else
     {
      *theInstance = (Instance *)
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->matchingItem;
      *theMarkers =
        get_nth_pm_match(EngineData(theEnv)->GlobalLHSBinds,pattern)->markers;
     }
  }

/***************************************************
  NAME         : GetObjectValueGeneral
  DESCRIPTION  : Access function for getting
                 pattern variable values within the
                 object pattern and join networks
  INPUTS       : 1) The result data object buffer
                 2) The instance to access
                 3) The list of multifield markers
                    for the pattern
                 4) Data for variable reference
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data object is filled with the
                 values of the pattern variable
  NOTES        : None
 ***************************************************/
static void GetObjectValueGeneral(
  Environment *theEnv,
  CLIPSValue *returnValue,
  Instance *theInstance,
  struct multifieldMarker *theMarks,
  struct ObjectMatchVar1 *matchVar)
  {
   long field, extent; /* 6.04 Bug Fix */
   INSTANCE_SLOT **insSlot,*basisSlot;
   
   if (matchVar->objectAddress)
     {
      returnValue->type = INSTANCE_ADDRESS;
      returnValue->value = theInstance;
      return;
     }
   if (matchVar->whichSlot == ISA_ID)
     {
      returnValue->type = SYMBOL;
      returnValue->value = GetDefclassNamePointer(theInstance->cls);
      return;
     }
   if (matchVar->whichSlot == NAME_ID)
     {
      returnValue->type = INSTANCE_NAME;
      returnValue->value = theInstance->name;
      return;
     }
   insSlot =
     &theInstance->slotAddresses
     [theInstance->cls->slotNameMap[matchVar->whichSlot] - 1];

   /* =========================================
      We need to reference the basis slots if
      the slot of this object has changed while
      the RHS was executing

      However, if the reference is being done
      by the LHS of a rule (as a consequence of
      an RHS action), give the pattern matcher
      the real value of the slot
      ========================================= */
   if ((theInstance->basisSlots != NULL) &&
       (! EngineData(theEnv)->JoinOperationInProgress))
     {
      basisSlot = theInstance->basisSlots + (insSlot - theInstance->slotAddresses);
      if (basisSlot->value != NULL)
        insSlot = &basisSlot;
     }

   /* ==================================================
      If we know we are accessing the entire slot,
      the don't bother with searching multifield markers
      or calculating offsets
      ================================================== */
   if (matchVar->allFields)
     {
      returnValue->type = (unsigned short) (*insSlot)->type;
      returnValue->value = (*insSlot)->value;
      if (returnValue->type == MULTIFIELD)
        {
         returnValue->begin = 0;
         SetpDOEnd(returnValue,GetMFLength((*insSlot)->value));
        }
      return;
     }

   /* =============================================
      Access a general field in a slot pattern with
      two or more multifield variables
      ============================================= */
   field = CalculateSlotField(theMarks,*insSlot,matchVar->whichField,&extent);
   if (extent == -1)
     {
      if ((*insSlot)->desc->multiple)
        {
         returnValue->type = GetMFType((*insSlot)->value,field);
         returnValue->value = GetMFValue((*insSlot)->value,field);
        }
      else
        {
         returnValue->type = (unsigned short) (*insSlot)->type;
         returnValue->value = (*insSlot)->value;
        }
     }
   else
     {
      returnValue->type = MULTIFIELD;
      returnValue->value = (*insSlot)->value;
      returnValue->begin = field - 1;
      returnValue->end = field + extent - 2;
     }
  }

/***************************************************
  NAME         : GetObjectValueSimple
  DESCRIPTION  : Access function for getting
                 pattern variable values within the
                 object pattern and join networks
  INPUTS       : 1) The result data object buffer
                 2) The instance to access
                 3) Data for variable reference
  RETURNS      : Nothing useful
  SIDE EFFECTS : Data object is filled with the
                 values of the pattern variable
  NOTES        : None
 ***************************************************/
static void GetObjectValueSimple(
  Environment *theEnv,
  CLIPSValue *returnValue,
  Instance *theInstance,
  struct ObjectMatchVar2 *matchVar)
  {
   INSTANCE_SLOT **insSlot,*basisSlot;
   SEGMENT *segmentPtr;
   FIELD *fieldPtr;
   
   insSlot =
     &theInstance->slotAddresses
     [theInstance->cls->slotNameMap[matchVar->whichSlot] - 1];

   /* =========================================
      We need to reference the basis slots if
      the slot of this object has changed while
      the RHS was executing

      However, if the reference is being done
      by the LHS of a rule (as a consequence of
      an RHS action), give the pattern matcher
      the real value of the slot
      ========================================= */
   if ((theInstance->basisSlots != NULL) &&
       (! EngineData(theEnv)->JoinOperationInProgress))
     {
      basisSlot = theInstance->basisSlots + (insSlot - theInstance->slotAddresses);
      if (basisSlot->value != NULL)
        insSlot = &basisSlot;
     }

   if ((*insSlot)->desc->multiple)
     {
      segmentPtr = (SEGMENT *) (*insSlot)->value;
      if (matchVar->fromBeginning)
        {
         if (matchVar->fromEnd)
           {
            returnValue->type = MULTIFIELD;
            returnValue->value = segmentPtr;
            returnValue->begin = matchVar->beginningOffset;
            SetpDOEnd(returnValue,GetMFLength(segmentPtr) - matchVar->endOffset);
           }
         else
           {
            fieldPtr = &segmentPtr->theFields[matchVar->beginningOffset];
            returnValue->type = fieldPtr->type;
            returnValue->value = fieldPtr->value;
           }
        }
      else
        {
         fieldPtr = &segmentPtr->theFields[segmentPtr->multifieldLength -
                                           (matchVar->endOffset + 1)];
         returnValue->type = fieldPtr->type;
         returnValue->value = fieldPtr->value;
        }
     }
   else
     {
      returnValue->type = (unsigned short) (*insSlot)->type;
      returnValue->value = (*insSlot)->value;
     }
  }

/****************************************************
  NAME         : CalculateSlotField
  DESCRIPTION  : Determines the actual index into the
                 an object slot for a given pattern
                 variable
  INPUTS       : 1) The list of markers to examine
                 2) The instance slot (can be NULL)
                 3) The pattern index of the variable
                 4) A buffer in which to store the
                    extent of the pattern variable
                    (-1 for single-field vars)
  RETURNS      : The actual index
  SIDE EFFECTS : None
  NOTES        : None
 ****************************************************/
static long CalculateSlotField(
  struct multifieldMarker *theMarkers,
  INSTANCE_SLOT *theSlot,
  long theIndex,
  long *extent)
  {
   long actualIndex;
   void *theSlotName;

   actualIndex = theIndex;
   *extent = -1;
   if (theSlot == NULL)
     return(actualIndex);
   theSlotName = theSlot->desc->slotName->name;
   while (theMarkers != NULL)
     {
      if (theMarkers->where.whichSlot == theSlotName)
        break;
      theMarkers = theMarkers->next;
     }
   while ((theMarkers != NULL) ? (theMarkers->where.whichSlot == theSlotName) : false)
     {
      if (theMarkers->whichField == theIndex)
        {
         *extent = theMarkers->endPosition - theMarkers->startPosition + 1;
         return(actualIndex);
        }
      if (theMarkers->whichField > theIndex)
        return(actualIndex);
      actualIndex += theMarkers->endPosition - theMarkers->startPosition;
      theMarkers = theMarkers->next;
     }
   return(actualIndex);
  }

/****************************************************
  NAME         : GetInsMultiSlotField
  DESCRIPTION  : Gets the values of simple single
                 field references in multifield
                 slots for Rete comparisons
  INPUTS       : 1) A multifield field structure
                    to store the type and value in
                 2) The instance
                 3) The id of the slot
                 4) A flag indicating if offset is
                    from beginning or end of
                    multifield slot
                 5) The offset
  RETURNS      : The multifield field
  SIDE EFFECTS : None
  NOTES        : Should only be used to access
                 single-field reference in multifield
                 slots for pattern and join network
                 comparisons
 ****************************************************/
static void GetInsMultiSlotField(
  FIELD *theField,
  Instance *theInstance,
  unsigned theSlotID,
  unsigned fromBeginning,
  unsigned offset)
  {
   INSTANCE_SLOT * insSlot;
   SEGMENT *theSegment;
   FIELD *tmpField;

   insSlot = theInstance->slotAddresses
               [theInstance->cls->slotNameMap[theSlotID] - 1];

   /* Bug fix for 6.05 */

   if (insSlot->desc->multiple)
     {
      theSegment = (SEGMENT *) insSlot->value;
      if (fromBeginning)
        tmpField = &theSegment->theFields[offset];
      else
        tmpField = &theSegment->theFields[theSegment->multifieldLength - offset - 1];
      theField->type = tmpField->type;
      theField->value = tmpField->value;
     }
   else
     {
      theField->type = (unsigned short) insSlot->type;
      theField->value = insSlot->value;
     }
  }

#endif

