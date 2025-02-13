   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*             EXPRESSION BSAVE/BLOAD MODULE           */
   /*******************************************************/

/*************************************************************/
/* Purpose: Implements the binary save/load feature for the  */
/*    expression data structure.                             */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE)

#include <stdio.h>

#include "bload.h"
#include "bsave.h"
#include "constrct.h"
#include "dffctdef.h"
#include "envrnmnt.h"
#include "extnfunc.h"
#include "memalloc.h"
#include "moduldef.h"

#if DEFRULE_CONSTRUCT
#include "network.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrcbin.h"
#endif

#if DEFFUNCTION_CONSTRUCT
#include "dffnxbin.h"
#endif

#if DEFTEMPLATE_CONSTRUCT
#include "tmpltbin.h"
#endif

#if DEFGLOBAL_CONSTRUCT
#include "globlbin.h"
#endif

#if OBJECT_SYSTEM
#include "objbin.h"
#include "insfun.h"
#include "inscom.h"
#endif

#include "exprnbin.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static void                        UpdateExpression(Environment *,void *,long);

/***********************************************************/
/* AllocateExpressions: Determines the amount of space     */
/*   required for loading the binary image of expressions  */
/*   and allocates that amount of space.                   */
/***********************************************************/
void AllocateExpressions(
  Environment *theEnv)
  {
   size_t space;

   GenReadBinary(theEnv,&ExpressionData(theEnv)->NumberOfExpressions,sizeof(long));
   if (ExpressionData(theEnv)->NumberOfExpressions == 0L)
     ExpressionData(theEnv)->ExpressionArray = NULL;
   else
     {
      space = ExpressionData(theEnv)->NumberOfExpressions * sizeof(struct expr);
      ExpressionData(theEnv)->ExpressionArray = (struct expr *) genalloc(theEnv,space);
     }
  }

/**********************************************/
/* RefreshExpressions: Refreshes the pointers */
/*   used by the expression binary image.     */
/**********************************************/
void RefreshExpressions(
  Environment *theEnv)
  {
   if (ExpressionData(theEnv)->ExpressionArray == NULL) return;

   BloadandRefresh(theEnv,ExpressionData(theEnv)->NumberOfExpressions,
                   (unsigned) sizeof(BSAVE_EXPRESSION),UpdateExpression);
  }

/*********************************************************
  NAME         : UpdateExpression
  DESCRIPTION  : Given a bloaded expression buffer,
                   this routine refreshes the pointers
                   in the expression array
  INPUTS       : 1) a bloaded expression buffer
                 2) the index of the expression to refresh
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expression updated
  NOTES        : None
 *********************************************************/
static void UpdateExpression(
  Environment *theEnv,
  void *buf,
  long obji)
  {
   BSAVE_EXPRESSION *bexp;
   long theIndex;

   bexp = (BSAVE_EXPRESSION *) buf;
   ExpressionData(theEnv)->ExpressionArray[obji].type = bexp->type;
   switch(bexp->type)
     {
      case FCALL:
        ExpressionData(theEnv)->ExpressionArray[obji].value = BloadData(theEnv)->FunctionArray[bexp->value];
        break;

      case GCALL:
#if DEFGENERIC_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = GenericPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case PCALL:
#if DEFFUNCTION_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DeffunctionPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case DEFTEMPLATE_PTR:
#if DEFTEMPLATE_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DeftemplatePointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

     case DEFCLASS_PTR:
#if OBJECT_SYSTEM
        ExpressionData(theEnv)->ExpressionArray[obji].value = DefclassPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;

      case DEFGLOBAL_PTR:

#if DEFGLOBAL_CONSTRUCT
        ExpressionData(theEnv)->ExpressionArray[obji].value = DefglobalPointer(bexp->value);
#else
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
#endif
        break;


      case INTEGER:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->IntegerArray[bexp->value];
        IncrementIntegerCount((INTEGER_HN *) ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;

      case FLOAT:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->FloatArray[bexp->value];
        IncrementFloatCount((FLOAT_HN *) ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;

      case INSTANCE_NAME:
#if ! OBJECT_SYSTEM
        ExpressionData(theEnv)->ExpressionArray[obji].type = SYMBOL;
#endif
      case GBL_VARIABLE:
      case SYMBOL:
      case STRING:
        ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->SymbolArray[bexp->value];
        IncrementSymbolCount((SYMBOL_HN *) ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;

#if DEFTEMPLATE_CONSTRUCT
      case FACT_ADDRESS:
        ExpressionData(theEnv)->ExpressionArray[obji].value = &FactData(theEnv)->DummyFact;
        EnvIncrementFactCount(theEnv,ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;
#endif

#if OBJECT_SYSTEM
      case INSTANCE_ADDRESS:
        ExpressionData(theEnv)->ExpressionArray[obji].value = &InstanceData(theEnv)->DummyInstance;
        EnvIncrementInstanceCount(theEnv,ExpressionData(theEnv)->ExpressionArray[obji].value);
        break;
#endif

      case EXTERNAL_ADDRESS:
        ExpressionData(theEnv)->ExpressionArray[obji].value = NULL;
        break;

      case RVOID:
        break;

      default:
        if (EvaluationData(theEnv)->PrimitivesArray[bexp->type] == NULL) break;
        if (EvaluationData(theEnv)->PrimitivesArray[bexp->type]->bitMap)
          {
           ExpressionData(theEnv)->ExpressionArray[obji].value = SymbolData(theEnv)->BitMapArray[bexp->value];
           IncrementBitMapCount((BITMAP_HN *) ExpressionData(theEnv)->ExpressionArray[obji].value);
          }
        break;
     }

   theIndex = (long int) bexp->nextArg;
   if (theIndex == -1L)
     { ExpressionData(theEnv)->ExpressionArray[obji].nextArg = NULL; }
   else
     { ExpressionData(theEnv)->ExpressionArray[obji].nextArg = (struct expr *) &ExpressionData(theEnv)->ExpressionArray[theIndex]; }

   theIndex = (long int) bexp->argList;
   if (theIndex == -1L)
     { ExpressionData(theEnv)->ExpressionArray[obji].argList = NULL; }
   else
     { ExpressionData(theEnv)->ExpressionArray[obji].argList = (struct expr *) &ExpressionData(theEnv)->ExpressionArray[theIndex]; }
  }

/*********************************************/
/* ClearBloadedExpressions: Clears the space */
/*   utilized by an expression binary image. */
/*********************************************/
void ClearBloadedExpressions(
  Environment *theEnv)
  {
   unsigned long int i;
   size_t space;

   /*===============================================*/
   /* Update the busy counts of atomic data values. */
   /*===============================================*/

   for (i = 0; i < (unsigned long) ExpressionData(theEnv)->NumberOfExpressions; i++)
     {
      switch (ExpressionData(theEnv)->ExpressionArray[i].type)
        {
         case SYMBOL          :
         case STRING          :
         case INSTANCE_NAME   :
         case GBL_VARIABLE    :
           DecrementSymbolCount(theEnv,(SYMBOL_HN *) ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
         case FLOAT           :
           DecrementFloatCount(theEnv,(FLOAT_HN *) ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
         case INTEGER         :
           DecrementIntegerCount(theEnv,(INTEGER_HN *) ExpressionData(theEnv)->ExpressionArray[i].value);
           break;

#if DEFTEMPLATE_CONSTRUCT
         case FACT_ADDRESS    :
           EnvDecrementFactCount(theEnv,ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
#endif

#if OBJECT_SYSTEM
         case INSTANCE_ADDRESS :
           EnvDecrementInstanceCount(theEnv,ExpressionData(theEnv)->ExpressionArray[i].value);
           break;
#endif

         case RVOID:
           break;

         default:
           if (EvaluationData(theEnv)->PrimitivesArray[ExpressionData(theEnv)->ExpressionArray[i].type] == NULL) break;
           if (EvaluationData(theEnv)->PrimitivesArray[ExpressionData(theEnv)->ExpressionArray[i].type]->bitMap)
             { DecrementBitMapCount(theEnv,(BITMAP_HN *) ExpressionData(theEnv)->ExpressionArray[i].value); }
           break;
        }
     }

   /*===================================*/
   /* Free the binary expression array. */
   /*===================================*/

   space = ExpressionData(theEnv)->NumberOfExpressions * sizeof(struct expr);
   if (space != 0) genfree(theEnv,ExpressionData(theEnv)->ExpressionArray,space);
   ExpressionData(theEnv)->ExpressionArray = 0;
  }


#if BLOAD_AND_BSAVE

/***************************************************
  NAME         : FindHashedExpressions
  DESCRIPTION  : Sets the bsave expression array
                 indices for hashed expression nodes
                 and marks the items needed by
                 these expressions
  INPUTS       : None
  RETURNS      : Nothing useful
  SIDE EFFECTS : Atoms marked and ids set
  NOTES        : None
 ***************************************************/
void FindHashedExpressions(
  Environment *theEnv)
  {
   unsigned i;
   EXPRESSION_HN *exphash;

   for (i = 0 ; i < EXPRESSION_HASH_SIZE ; i++)
     for (exphash = ExpressionData(theEnv)->ExpressionHashTable[i] ; exphash != NULL ; exphash = exphash->next)
       {
        MarkNeededItems(theEnv,exphash->exp);
        exphash->bsaveID = ExpressionData(theEnv)->ExpressionCount;
        ExpressionData(theEnv)->ExpressionCount += ExpressionSize(exphash->exp);
       }
  }

/***************************************************
  NAME         : BsaveHashedExpressions
  DESCRIPTION  : Writes out hashed expressions
  INPUTS       : Bsave file stream pointer
  RETURNS      : Nothing useful
  SIDE EFFECTS : Expressions written
  NOTES        : None
 ***************************************************/
void BsaveHashedExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   unsigned i;
   EXPRESSION_HN *exphash;

   for (i = 0 ; i < EXPRESSION_HASH_SIZE ; i++)
     for (exphash = ExpressionData(theEnv)->ExpressionHashTable[i] ; exphash != NULL ; exphash = exphash->next)
       BsaveExpression(theEnv,exphash->exp,fp);
  }

/***************************************************************/
/* BsaveConstructExpressions: Writes all expression needed by  */
/*   constructs for this binary image to the binary save file. */
/***************************************************************/
void BsaveConstructExpressions(
  Environment *theEnv,
  FILE *fp)
  {
   struct BinaryItem *biPtr;

   for (biPtr = BsaveData(theEnv)->ListOfBinaryItems;
        biPtr != NULL;
        biPtr = biPtr->next)
     {
      if (biPtr->expressionFunction != NULL)
        { (*biPtr->expressionFunction)(theEnv,fp); }
     }
  }

/***************************************/
/* BsaveExpression: Recursively saves  */
/*   an expression to the binary file. */
/***************************************/
void BsaveExpression(
  Environment *theEnv,
  struct expr *testPtr,
  FILE *fp)
  {
   BSAVE_EXPRESSION newTest;
   long int newIndex;

   while (testPtr != NULL)
     {
      ExpressionData(theEnv)->ExpressionCount++;

      /*================*/
      /* Copy the type. */
      /*================*/

      newTest.type = testPtr->type;

      /*=======================================*/
      /* Convert the argList slot to an index. */
      /*=======================================*/

      if (testPtr->argList == NULL)
        { newTest.argList = -1L; }
      else
        { newTest.argList = ExpressionData(theEnv)->ExpressionCount; }

      /*========================================*/
      /* Convert the nextArg slot to an index. */
      /*========================================*/

      if (testPtr->nextArg == NULL)
        { newTest.nextArg = -1L; }
      else
        {
         newIndex = ExpressionData(theEnv)->ExpressionCount + ExpressionSize(testPtr->argList);
         newTest.nextArg = newIndex;
        }

      /*=========================*/
      /* Convert the value slot. */
      /*=========================*/

      switch(testPtr->type)
        {
         case FLOAT:
           newTest.value = (long) ((FLOAT_HN *) testPtr->value)->bucket;
           break;

         case INTEGER:
           newTest.value = (long) ((INTEGER_HN *) testPtr->value)->bucket;
           break;

         case FCALL:
           newTest.value = (long) ((struct FunctionDefinition *)
                                   testPtr->value)->bsaveIndex;
           break;

         case GCALL:
#if DEFGENERIC_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = ((struct constructHeader *) testPtr->value)->bsaveID;
           else
#endif
             newTest.value = -1L;
           break;

         case PCALL:
#if DEFFUNCTION_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = ((struct constructHeader *) testPtr->value)->bsaveID;
           else
#endif
             newTest.value = -1L;
           break;

         case DEFTEMPLATE_PTR:
#if DEFTEMPLATE_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = ((struct constructHeader *) testPtr->value)->bsaveID;
           else
#endif
             newTest.value = -1L;
           break;

         case DEFCLASS_PTR:
#if OBJECT_SYSTEM
           if (testPtr->value != NULL)
             newTest.value = ((struct constructHeader *) testPtr->value)->bsaveID;
           else
#endif
             newTest.value = -1L;
           break;

         case DEFGLOBAL_PTR:
#if DEFGLOBAL_CONSTRUCT
           if (testPtr->value != NULL)
             newTest.value = ((Defglobal *) testPtr->value)->header.bsaveID;
           else
#endif
             newTest.value = -1L;
           break;

#if OBJECT_SYSTEM
         case INSTANCE_NAME:
#endif
         case SYMBOL:
         case GBL_VARIABLE:
         case STRING:
           newTest.value = (long) ((SYMBOL_HN *) testPtr->value)->bucket;
           break;

         case FACT_ADDRESS:
         case INSTANCE_ADDRESS:
         case EXTERNAL_ADDRESS:
           newTest.value = -1L;
           break;

         case RVOID:
           break;

         default:
           if (EvaluationData(theEnv)->PrimitivesArray[testPtr->type] == NULL) break;
           if (EvaluationData(theEnv)->PrimitivesArray[testPtr->type]->bitMap)
             { newTest.value = (long) ((BITMAP_HN *) testPtr->value)->bucket; }
           break;
        }

     /*===========================*/
     /* Write out the expression. */
     /*===========================*/

     GenWrite(&newTest,(unsigned long) sizeof(BSAVE_EXPRESSION),fp);

     /*==========================*/
     /* Write out argument list. */
     /*==========================*/

     if (testPtr->argList != NULL)
       {
        BsaveExpression(theEnv,testPtr->argList,fp);
       }

     testPtr = testPtr->nextArg;
    }
  }

#endif /* BLOAD_AND_BSAVE */

#endif /* (BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE) */

