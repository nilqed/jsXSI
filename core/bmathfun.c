   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  08/25/16             */
   /*                                                     */
   /*             BASIC MATH FUNCTIONS MODULE             */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for numerous basic math        */
/*   functions including +, *, -, /, integer, float, div,    */
/*   abs,set-auto-float-dividend, get-auto-float-dividend,   */
/*   min, and max.                                           */
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
/*            Converted API macros to function calls.        */
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
/*            ALLOW_ENVIRONMENT_GLOBALS no longer supported. */
/*                                                           */
/*            UDF redesign.                                  */
/*                                                           */
/*            Auto-float-dividend always enabled.            */
/*                                                           */
/*************************************************************/

#include <stdio.h>

#include "setup.h"

#include "argacces.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "router.h"

#include "bmathfun.h"

/***************************************************************/
/* BasicMathFunctionDefinitions: Defines basic math functions. */
/***************************************************************/
void BasicMathFunctionDefinitions(
  Environment *theEnv)
  {
#if ! RUN_TIME
   EnvAddUDF(theEnv,"+","ld",2,UNBOUNDED,"ld",AdditionFunction,"AdditionFunction",NULL);
   EnvAddUDF(theEnv,"*","ld",2,UNBOUNDED,"ld",MultiplicationFunction,"MultiplicationFunction",NULL);
   EnvAddUDF(theEnv,"-","ld",2,UNBOUNDED,"ld",SubtractionFunction,"SubtractionFunction",NULL);
   EnvAddUDF(theEnv,"/","d",2,UNBOUNDED,"ld",DivisionFunction,"DivisionFunction",NULL);
   EnvAddUDF(theEnv,"div","l",2,UNBOUNDED,"ld",DivFunction,"DivFunction",NULL);
   EnvAddUDF(theEnv,"integer","l",1,1,"ld",IntegerFunction,"IntegerFunction",NULL);
   EnvAddUDF(theEnv,"float","d",1,1,"ld",FloatFunction,"FloatFunction",NULL);
   EnvAddUDF(theEnv,"abs","ld",1,1,"ld",AbsFunction,"AbsFunction",NULL);
   EnvAddUDF(theEnv,"min","ld",1,UNBOUNDED,"ld",MinFunction,"MinFunction",NULL);
   EnvAddUDF(theEnv,"max","ld",1,UNBOUNDED,"ld",MaxFunction,"MaxFunction",NULL);
#endif
  }

/**********************************/
/* AdditionFunction: H/L access   */
/*   routine for the + function.  */
/**********************************/
void AdditionFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSFloat ftotal = 0.0;
   CLIPSInteger ltotal = 0LL;
   bool useFloatTotal = false;
   CLIPSValue theArg;

   /*=================================================*/
   /* Loop through each of the arguments adding it to */
   /* a running total. If a floating point number is  */
   /* encountered, then do all subsequent operations  */
   /* using floating point values.                    */
   /*=================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal += mCVToFloat(&theArg); }
      else
        {
         if (mCVIsType(&theArg,INTEGER_TYPE))
           { ltotal += mCVToInteger(&theArg); }
         else
           {
            ftotal = ((CLIPSFloat) ltotal) + mCVToFloat(&theArg);
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { mCVSetFloat(returnValue,ftotal); }
   else
     { mCVSetInteger(returnValue,ltotal); }
  }

/****************************************/
/* MultiplicationFunction: CLIPS access */
/*   routine for the * function.        */
/****************************************/
void MultiplicationFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSFloat ftotal = 1.0;
   CLIPSInteger ltotal = 1LL;
   bool useFloatTotal = false;
   CLIPSValue theArg;

   /*===================================================*/
   /* Loop through each of the arguments multiplying it */
   /* by a running product. If a floating point number  */
   /* is encountered, then do all subsequent operations */
   /* using floating point values.                      */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal *= mCVToFloat(&theArg); }
      else
        {
         if (mCVIsType(&theArg,INTEGER_TYPE))
           { ltotal *= mCVToInteger(&theArg); }
         else
           {
            ftotal = ((CLIPSFloat) ltotal) * mCVToFloat(&theArg);
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { mCVSetFloat(returnValue,ftotal); }
   else
     { mCVSetInteger(returnValue,ltotal); }
  }

/*************************************/
/* SubtractionFunction: CLIPS access */
/*   routine for the - function.     */
/*************************************/
void SubtractionFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSFloat ftotal = 0.0;
   CLIPSInteger ltotal = 0LL;
   bool useFloatTotal = false;
   CLIPSValue theArg;

   /*=================================================*/
   /* Get the first argument. This number which will  */
   /* be the starting total from which all subsequent */
   /* arguments will subtracted.                      */
   /*=================================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   if (mCVIsType(&theArg,INTEGER_TYPE))
     { ltotal = mCVToInteger(&theArg); }
   else
     {
      ftotal = mCVToFloat(&theArg);
      useFloatTotal = true;
     }

   /*===================================================*/
   /* Loop through each of the arguments subtracting it */
   /* from a running total. If a floating point number  */
   /* is encountered, then do all subsequent operations */
   /* using floating point values.                      */
   /*===================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
        { return; }

      if (useFloatTotal)
        { ftotal -= mCVToFloat(&theArg); }
      else
        {
         if (mCVIsType(&theArg,INTEGER_TYPE))
           { ltotal -= mCVToInteger(&theArg); }
         else
           {
            ftotal = ((CLIPSFloat) ltotal) - mCVToFloat(&theArg);
            useFloatTotal = true;
           }
        }
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   if (useFloatTotal)
     { mCVSetFloat(returnValue,ftotal); }
   else
     { mCVSetInteger(returnValue,ltotal); }
  }

/***********************************/
/* DivisionFunction:  CLIPS access */
/*   routine for the / function.   */
/***********************************/
void DivisionFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSFloat ftotal = 1.0;
   CLIPSFloat theNumber;
   CLIPSValue theArg;
   
   /*===================================================*/
   /* Get the first argument. This number which will be */
   /* the starting product from which all subsequent    */
   /* arguments will divide. If the auto float dividend */
   /* feature is enable, then this number is converted  */
   /* to a float if it is an integer.                   */
   /*===================================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }

   ftotal = mCVToFloat(&theArg);

   /*====================================================*/
   /* Loop through each of the arguments dividing it     */
   /* into a running product. If a floating point number */
   /* is encountered, then do all subsequent operations  */
   /* using floating point values. Each argument is      */
   /* checked to prevent a divide by zero error.         */
   /*====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
        { return; }
        
      theNumber = mCVToFloat(&theArg);
      
      if (theNumber == 0.0)
        {
         DivideByZeroErrorMessage(theEnv,"/");
         EnvSetEvaluationError(theEnv,true);
         mCVSetFloat(returnValue,1.0);
         return;
        }

      ftotal /= theNumber;
     }

   /*======================================================*/
   /* If a floating point number was in the argument list, */
   /* then return a float, otherwise return an integer.    */
   /*======================================================*/

   mCVSetFloat(returnValue,ftotal);
  }

/*************************************/
/* DivFunction: H/L access routine   */
/*   for the div function.           */
/*************************************/
void DivFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSInteger total = 1LL;
   CLIPSValue theArg;
   CLIPSInteger theNumber;

   /*===================================================*/
   /* Get the first argument. This number which will be */
   /* the starting product from which all subsequent    */
   /* arguments will divide.                            */
   /*===================================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,&theArg))
     { return; }
   total = mCVToInteger(&theArg);

   /*=====================================================*/
   /* Loop through each of the arguments dividing it into */
   /* a running product. Floats are converted to integers */
   /* and each argument is checked to prevent a divide by */
   /* zero error.                                         */
   /*=====================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&theArg))
        { return; }

      theNumber = mCVToInteger(&theArg);

      if (theNumber == 0LL)
        {
         DivideByZeroErrorMessage(theEnv,"div");
         EnvSetEvaluationError(theEnv,true);
         mCVSetInteger(returnValue,1L);
         return;
        }

      total /= theNumber;
     }

   /*======================================================*/
   /* The result of the div function is always an integer. */
   /*======================================================*/

   mCVSetInteger(returnValue,total);
  }

/*****************************************/
/* IntegerFunction: H/L access routine   */
/*   for the integer function.           */
/*****************************************/
void IntegerFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! UDFNthArgument(context,1,NUMBER_TYPES,returnValue))
     { return; }

   /*============================================*/
   /* Convert a float type to integer, otherwise */
   /* return the argument unchanged.             */
   /*============================================*/

   if (mCVIsType(returnValue,FLOAT_TYPE))
     { mCVSetInteger(returnValue,mCVToInteger(returnValue)); }
  }

/***************************************/
/* FloatFunction: H/L access routine   */
/*   for the float function.           */
/***************************************/
void FloatFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! UDFNthArgument(context,1,NUMBER_TYPES,returnValue))
     { return; }

   /*=============================================*/
   /* Convert an integer type to float, otherwise */
   /* return the argument unchanged.              */
   /*=============================================*/

   if (mCVIsType(returnValue,INTEGER_TYPE))
     { mCVSetFloat(returnValue,mCVToFloat(returnValue)); }
  }

/*************************************/
/* AbsFunction: H/L access routine   */
/*   for the abs function.           */
/*************************************/
void AbsFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   /*======================================*/
   /* Check that the argument is a number. */
   /*======================================*/

   if (! UDFNthArgument(context,1,NUMBER_TYPES,returnValue))
     { return; }

   /*==========================================*/
   /* Return the absolute value of the number. */
   /*==========================================*/

   if (mCVIsType(returnValue,INTEGER_TYPE))
     {
      CLIPSInteger lv = mCVToInteger(returnValue);
      if (lv < 0L) mCVSetInteger(returnValue,-lv);
     }
   else
     {
      CLIPSFloat dv = mCVToFloat(returnValue);
      if (dv < 0.0) mCVSetFloat(returnValue,-dv);
     }
  }

/*************************************/
/* MinFunction: H/L access routine   */
/*   for the min function.           */
/*************************************/
void MinFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue nextPossible;

   /*============================================*/
   /* Check that the first argument is a number. */
   /*============================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,returnValue))
     { return; }

   /*===========================================================*/
   /* Loop through the remaining arguments, first checking each */
   /* argument to see that it is a number, and then determining */
   /* if the argument is less than the previous arguments and   */
   /* is thus the minimum value.                                */
   /*===========================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&nextPossible))
        { return; }
      
      /*=============================================*/
      /* If either argument is a float, convert both */
      /* to floats. Otherwise compare two integers.  */
      /*=============================================*/
      
      if (mCVIsType(returnValue,FLOAT_TYPE) || mCVIsType(&nextPossible,FLOAT_TYPE))
        {
         if (mCVToFloat(returnValue) > mCVToFloat(&nextPossible))
           { CVSetCLIPSValue(returnValue,&nextPossible); }
        }
      else
        {
         if (mCVToInteger(returnValue) > mCVToInteger(&nextPossible))
           { CVSetCLIPSValue(returnValue,&nextPossible); }
        }
     }
  }

/*************************************/
/* MaxFunction: H/L access routine   */
/*   for the max function.           */
/*************************************/
void MaxFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue nextPossible;
   
   /*============================================*/
   /* Check that the first argument is a number. */
   /*============================================*/

   if (! UDFFirstArgument(context,NUMBER_TYPES,returnValue))
     { return; }

   /*===========================================================*/
   /* Loop through the remaining arguments, first checking each */
   /* argument to see that it is a number, and then determining */
   /* if the argument is greater than the previous arguments    */
   /* and is thus the maximum value.                            */
   /*===========================================================*/

   while (UDFHasNextArgument(context))
     {
      if (! UDFNextArgument(context,NUMBER_TYPES,&nextPossible))
        { return; }
      
      /*=============================================*/
      /* If either argument is a float, convert both */
      /* to floats. Otherwise compare two integers.  */
      /*=============================================*/
      
      if (mCVIsType(returnValue,FLOAT_TYPE) || mCVIsType(&nextPossible,FLOAT_TYPE))
        {
         if (mCVToFloat(returnValue) < mCVToFloat(&nextPossible))
           { CVSetCLIPSValue(returnValue,&nextPossible); }
        }
      else
        {
         if (mCVToInteger(returnValue) < mCVToInteger(&nextPossible))
           { CVSetCLIPSValue(returnValue,&nextPossible); }
        }
     }
  }

