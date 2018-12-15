   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*              PREDICATE FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several predicate          */
/*   functions including not, and, or, eq, neq, <=, >=, <,   */
/*   >, =, <>, symbolp, stringp, lexemep, numberp, integerp, */
/*   floatp, oddp, evenp, multifieldp, sequencep, and        */
/*   pointerp.                                               */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Support for long long integers.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
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

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "multifld.h"
#include "router.h"

#include "prdctfun.h"

/**************************************************/
/* PredicateFunctionDefinitions: Defines standard */
/*   math and predicate functions.                */
/**************************************************/
void PredicateFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   EnvAddUDF(theEnv,"not","b",1,1,NULL,NotFunction,"NotFunction",NULL);
   EnvAddUDF(theEnv,"and","b",2,UNBOUNDED ,NULL,AndFunction,"AndFunction",NULL);
   EnvAddUDF(theEnv,"or","b",2,UNBOUNDED ,NULL,OrFunction,"OrFunction",NULL);

   EnvAddUDF(theEnv,"eq","b",2,UNBOUNDED,NULL,EqFunction,"EqFunction",NULL);
   EnvAddUDF(theEnv,"neq","b",2,UNBOUNDED,NULL,NeqFunction,"NeqFunction",NULL);

   EnvAddUDF(theEnv,"<=","b",2,UNBOUNDED ,"ld",LessThanOrEqualFunction,"LessThanOrEqualFunction",NULL);
   EnvAddUDF(theEnv,">=","b",2,UNBOUNDED ,"ld",GreaterThanOrEqualFunction,"GreaterThanOrEqualFunction",NULL);
   EnvAddUDF(theEnv,"<","b",2,UNBOUNDED ,"ld",LessThanFunction,"LessThanFunction",NULL);
   EnvAddUDF(theEnv,">","b",2,UNBOUNDED ,"ld",GreaterThanFunction,"GreaterThanFunction",NULL);
   EnvAddUDF(theEnv,"=","b",2,UNBOUNDED ,"ld",NumericEqualFunction,"NumericEqualFunction",NULL);
   EnvAddUDF(theEnv,"<>","b",2,UNBOUNDED ,"ld",NumericNotEqualFunction,"NumericNotEqualFunction",NULL);
   EnvAddUDF(theEnv,"!=","b",2,UNBOUNDED ,"ld",NumericNotEqualFunction,"NumericNotEqualFunction",NULL);

   EnvAddUDF(theEnv,"symbolp","b",1,1,NULL,SymbolpFunction,"SymbolpFunction",NULL);
   EnvAddUDF(theEnv,"wordp","b",1,1,NULL,SymbolpFunction,"SymbolpFunction",NULL);  // TBD Remove?
   EnvAddUDF(theEnv,"stringp","b",1,1,NULL,StringpFunction,"StringpFunction",NULL);
   EnvAddUDF(theEnv,"lexemep","b",1,1,NULL,LexemepFunction,"LexemepFunction",NULL);
   EnvAddUDF(theEnv,"numberp","b",1,1,NULL,NumberpFunction,"NumberpFunction",NULL);
   EnvAddUDF(theEnv,"integerp","b",1,1,NULL,IntegerpFunction,"IntegerpFunction",NULL);
   EnvAddUDF(theEnv,"floatp","b",1,1,NULL,FloatpFunction,"FloatpFunction",NULL);
   EnvAddUDF(theEnv,"oddp","b",1,1,"l",OddpFunction,"OddpFunction",NULL);
   EnvAddUDF(theEnv,"evenp","b",1,1,"l",EvenpFunction,"EvenpFunction",NULL);
   EnvAddUDF(theEnv,"multifieldp","b",1,1,NULL,MultifieldpFunction,"MultifieldpFunction",NULL);
   EnvAddUDF(theEnv,"sequencep","b",1,1,NULL,MultifieldpFunction,"MultifieldpFunction",NULL); // TBD Remove?
   EnvAddUDF(theEnv,"pointerp","b",1,1,NULL,PointerpFunction,"PointerpFunction",NULL);
#else
#if MAC_XCD
#pragma unused(theEnv)
#endif
#endif
  }

/************************************/
/* EqFunction: H/L access routine   */
/*   for the eq function.           */
/************************************/
void EqFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item, nextItem;
   int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* Determine the number of arguments. */
   /*====================================*/

   numArgs = UDFArgumentCount(context);
   if (numArgs == 0)
     {
      mCVSetBoolean(returnValue,false);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are the same, return FALSE.  */
   /*=====================================*/

   theExpression = GetNextArgument(theExpression);
   for (i = 2 ; i <= numArgs ; i++)
     {
      EvaluateExpression(theEnv,theExpression,&nextItem);

      if (GetType(nextItem) != GetType(item))
        {
         mCVSetBoolean(returnValue,false);
         return;
        }

      if (GetType(nextItem) == MULTIFIELD)
        {
         if (MultifieldDOsEqual(&nextItem,&item) == false)
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else if (nextItem.value != item.value)
        {
         mCVSetBoolean(returnValue,false);
         return;
        }

      theExpression = GetNextArgument(theExpression);
     }

   /*=====================================*/
   /* All of the arguments were different */
   /* from the first. Return TRUE.        */
   /*=====================================*/

   mCVSetBoolean(returnValue,true);
  }

/*************************************/
/* NeqFunction: H/L access routine   */
/*   for the neq function.           */
/*************************************/
void NeqFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item, nextItem;
   int numArgs, i;
   struct expr *theExpression;

   /*====================================*/
   /* Determine the number of arguments. */
   /*====================================*/

   numArgs = UDFArgumentCount(context);
   if (numArgs == 0)
     {
      mCVSetBoolean(returnValue,false);
      return;
     }

   /*==============================================*/
   /* Get the value of the first argument against  */
   /* which subsequent arguments will be compared. */
   /*==============================================*/

   theExpression = GetFirstArgument();
   EvaluateExpression(theEnv,theExpression,&item);

   /*=====================================*/
   /* Compare all arguments to the first. */
   /* If any are different, return FALSE. */
   /*=====================================*/

   for (i = 2, theExpression = GetNextArgument(theExpression);
        i <= numArgs;
        i++, theExpression = GetNextArgument(theExpression))
     {
      EvaluateExpression(theEnv,theExpression,&nextItem);
      if (GetType(nextItem) != GetType(item))
        { continue; }
      else if (nextItem.type == MULTIFIELD)
        {
         if (MultifieldDOsEqual(&nextItem,&item) == true)
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else if (nextItem.value == item.value)
        {
         mCVSetBoolean(returnValue,false);
         return;
        }
     }

   /*=====================================*/
   /* All of the arguments were identical */
   /* to the first. Return TRUE.          */
   /*=====================================*/

   mCVSetBoolean(returnValue,true);
  }

/*****************************************/
/* StringpFunction: H/L access routine   */
/*   for the stringp function.           */
/*****************************************/
void StringpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,STRING_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/*****************************************/
/* SymbolpFunction: H/L access routine   */
/*   for the symbolp function.           */
/*****************************************/
void SymbolpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,SYMBOL_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/*****************************************/
/* LexemepFunction: H/L access routine   */
/*   for the lexemep function.           */
/*****************************************/
void LexemepFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,LEXEME_TYPES))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/*****************************************/
/* NumberpFunction: H/L access routine   */
/*   for the numberp function.           */
/*****************************************/
void NumberpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,NUMBER_TYPES))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/****************************************/
/* FloatpFunction: H/L access routine   */
/*   for the floatp function.           */
/****************************************/
void FloatpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,FLOAT_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/******************************************/
/* IntegerpFunction: H/L access routine   */
/*   for the integerp function.           */
/******************************************/
void IntegerpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,INTEGER_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/*********************************************/
/* MultifieldpFunction: H/L access routine   */
/*   for the multifieldp function.           */
/*********************************************/
void MultifieldpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,MULTIFIELD_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/******************************************/
/* PointerpFunction: H/L access routine   */
/*   for the pointerp function.           */
/******************************************/
void PointerpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;

   if (! UDFFirstArgument(context,ANY_TYPE,&item))
     { return; }

   if (mCVIsType(&item,EXTERNAL_ADDRESS_TYPE))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/***********************************/
/* NotFunction: H/L access routine */
/*   for the not function.         */
/***********************************/
void NotFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   if (! UDFFirstArgument(context,ANY_TYPE,&theArg))
     { return; }

   if (mCVIsFalseSymbol(&theArg))
     { mCVSetBoolean(returnValue,true); }
   else
     { mCVSetBoolean(returnValue,false); }
  }

/*************************************/
/* AndFunction: H/L access routine   */
/*   for the and function.           */
/*************************************/
void AndFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,ANY_TYPE,&theArg))
        { return; }
        
      if (mCVIsFalseSymbol(&theArg))
        {
         mCVSetBoolean(returnValue,false);
         return;
        }
     }

   mCVSetBoolean(returnValue,true);
  }

/************************************/
/* OrFunction: H/L access routine   */
/*   for the or function.           */
/************************************/
void OrFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,ANY_TYPE,&theArg))
        { return; }
        
      if (! mCVIsFalseSymbol(&theArg))
        {
         mCVSetBoolean(returnValue,true);
         return;
        }
     }

   mCVSetBoolean(returnValue,false);
  }

/*****************************************/
/* LessThanOrEqualFunction: H/L access   */
/*   routine for the <= function.        */
/*****************************************/
void LessThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*====================================================*/
   /* Compare each of the subsequent arguments to its    */
   /* predecessor. If any is greater, then return FALSE. */
   /*====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }

      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) > mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) > mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }

      CVSetCLIPSValue(&rv1,&rv2);
     }

   /*======================================*/
   /* Each argument was less than or equal */
   /* to its predecessor. Return TRUE.     */
   /*======================================*/

   mCVSetBoolean(returnValue,true);
  }

/********************************************/
/* GreaterThanOrEqualFunction: H/L access   */
/*   routine for the >= function.           */
/********************************************/
void GreaterThanOrEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*===================================================*/
   /* Compare each of the subsequent arguments to its   */
   /* predecessor. If any is lesser, then return false. */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }

      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) < mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) < mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }

      CVSetCLIPSValue(&rv1,&rv2);
     }

   /*=========================================*/
   /* Each argument was greater than or equal */
   /* to its predecessor. Return TRUE.        */
   /*=========================================*/

   mCVSetBoolean(returnValue,true);
  }

/**********************************/
/* LessThanFunction: H/L access   */
/*   routine for the < function.  */
/**********************************/
void LessThanFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is greater or */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }
        
      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) >= mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) >= mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
        
      CVSetCLIPSValue(&rv1,&rv2);
     }

   /*=================================*/
   /* Each argument was less than its */
   /* predecessor. Return TRUE.       */
   /*=================================*/

   mCVSetBoolean(returnValue,true);
  }

/*************************************/
/* GreaterThanFunction: H/L access   */
/*   routine for the > function.     */
/*************************************/
void GreaterThanFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*==========================================*/
   /* Compare each of the subsequent arguments */
   /* to its predecessor. If any is lesser or  */
   /* equal, then return FALSE.                */
   /*==========================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }
        
      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) <= mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) <= mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
        
      CVSetCLIPSValue(&rv1,&rv2);
     }

   /*================================*/
   /* Each argument was greater than */
   /* its predecessor. Return TRUE.  */
   /*================================*/

   mCVSetBoolean(returnValue,true);
  }

/**************************************/
/* NumericEqualFunction: H/L access   */
/*   routine for the = function.      */
/**************************************/
void NumericEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is unequal, then return FALSE.    */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }
        
      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) != mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) != mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
     }

   /*=================================*/
   /* All arguments were equal to the */
   /* first argument. Return TRUE.    */
   /*=================================*/

   mCVSetBoolean(returnValue,true);
  }

/*****************************************/
/* NumericNotEqualFunction: H/L access   */
/*   routine for the <> function.        */
/*****************************************/
void NumericNotEqualFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue rv1, rv2;

   /*=========================*/
   /* Get the first argument. */
   /*=========================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&rv1))
     { return; }

   /*=================================================*/
   /* Compare each of the subsequent arguments to the */
   /* first. If any is equal, then return FALSE.      */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&rv2))
        { return; }
        
      if (mCVIsType(&rv1,INTEGER_TYPE) && mCVIsType(&rv2,INTEGER_TYPE))
        {
         if (mCVToInteger(&rv1) == mCVToInteger(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
      else
        {
         if (mCVToFloat(&rv1) == mCVToFloat(&rv2))
           {
            mCVSetBoolean(returnValue,false);
            return;
           }
        }
     }

   /*===================================*/
   /* All arguments were unequal to the */
   /* first argument. Return TRUE.      */
   /*===================================*/

   mCVSetBoolean(returnValue,true);
  }

/**************************************/
/* OddpFunction: H/L access routine   */
/*   for the oddp function.           */
/**************************************/
void OddpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;
   CLIPSInteger num, halfnum;
      
   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/
 
   if (! UDFFirstArgument(context,INTEGER_TYPE,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/
   
   num = mCVToInteger(&item);
   halfnum = (num / 2) * 2;

   if (num == halfnum) mCVSetBoolean(returnValue,false);
   else mCVSetBoolean(returnValue,true);
  }

/***************************************/
/* EvenpFunction: H/L access routine   */
/*   for the evenp function.           */
/***************************************/
void EvenpFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue item;
   CLIPSInteger num, halfnum;
   
   /*===========================================*/
   /* Check for the correct types of arguments. */
   /*===========================================*/
     
   if (! UDFFirstArgument(context,INTEGER_TYPE,&item))
     { return; }

   /*===========================*/
   /* Compute the return value. */
   /*===========================*/
   
   num = mCVToInteger(&item);
   halfnum = (num / 2) * 2;
   
   if (num != halfnum) mCVSetBoolean(returnValue,false);
   else mCVSetBoolean(returnValue,true);
  }



