   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*               PARSING FUNCTIONS MODULE              */
   /*******************************************************/

/*************************************************************/
/* Purpose: Contains the code for several parsing related    */
/*   functions including...                                  */
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
/*      6.24: Corrected code to remove run-time program      */
/*            compiler warnings.                             */
/*                                                           */
/*      6.30: Changed integer type/precision.                */
/*                                                           */
/*            Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed function declaration issue when          */
/*            BLOAD_ONLY compiler flag is set to 1.          */
/*                                                           */
/*      6.40: Changed check-syntax router name because of    */
/*            a conflict with another error-capture router.  */
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
/*            UDF redesign.                                  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#include <string.h>

#include "argacces.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "multifld.h"
#include "prcdrpsr.h"
#include "router.h"
#include "strngrtr.h"
#include "utility.h"

#include "parsefun.h"

#define PARSEFUN_DATA 11

struct parseFunctionData
  { 
   char *ErrorString;
   size_t ErrorCurrentPosition;
   size_t ErrorMaximumPosition;
   char *WarningString;
   size_t WarningCurrentPosition;
   size_t WarningMaximumPosition;
  };

#define ParseFunctionData(theEnv) ((struct parseFunctionData *) GetEnvironmentData(theEnv,PARSEFUN_DATA))

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static bool                    FindErrorCapture(Environment *,const char *);
   static void                    PrintErrorCapture(Environment *,const char *,const char *);
   static void                    DeactivateErrorCapture(Environment *);
   static void                    SetErrorCaptureValues(Environment *,CLIPSValue *);
#endif

/*****************************************/
/* ParseFunctionDefinitions: Initializes */
/*   the parsing related functions.      */
/*****************************************/
void ParseFunctionDefinitions(
  Environment *theEnv)
  {
   AllocateEnvironmentData(theEnv,PARSEFUN_DATA,sizeof(struct parseFunctionData),NULL);

#if ! RUN_TIME
   EnvAddUDF(theEnv,"check-syntax","ym",1,1,"s",CheckSyntaxFunction,"CheckSyntaxFunction",NULL);
#endif
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)
/*******************************************/
/* CheckSyntaxFunction: H/L access routine */
/*   for the check-syntax function.        */
/*******************************************/
void CheckSyntaxFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   CLIPSValue theArg;

   /*========================================*/
   /* The argument should be of type STRING. */
   /*========================================*/

   if (! UDFFirstArgument(context,STRING_TYPE,&theArg))
     { return; }

   /*===================*/
   /* Check the syntax. */
   /*===================*/

   CheckSyntax(theEnv,mCVToString(&theArg),returnValue);
  }

/*********************************/
/* CheckSyntax: C access routine */
/*   for the build function.     */
/*********************************/
bool CheckSyntax(
  Environment *theEnv,
  const char *theString,
  CLIPSValue *returnValue)
  {
   const char *name;
   struct token theToken;
   struct expr *top;
   bool rv;

   /*==============================*/
   /* Set the default return value */
   /* (TRUE for problems found).   */
   /*==============================*/

   mCVSetBoolean(returnValue,true);

   /*===========================================*/
   /* Create a string source router so that the */
   /* string can be used as an input source.    */
   /*===========================================*/

   if (OpenStringSource(theEnv,"check-syntax",theString,0) == 0)
     { return true; }

   /*=================================*/
   /* Only expressions and constructs */
   /* can have their syntax checked.  */
   /*=================================*/

   GetToken(theEnv,"check-syntax",&theToken);

   if (theToken.type != LPAREN)
     {
      CloseStringSource(theEnv,"check-syntax");
      mCVSetSymbol(returnValue,"MISSING-LEFT-PARENTHESIS");
      return true;
     }

   /*========================================*/
   /* The next token should be the construct */
   /* type or function name.                 */
   /*========================================*/

   GetToken(theEnv,"check-syntax",&theToken);
   if (theToken.type != SYMBOL)
     {
      CloseStringSource(theEnv,"check-syntax");
      mCVSetSymbol(returnValue,"EXPECTED-SYMBOL-AFTER-LEFT-PARENTHESIS");
      return true;
     }

   name = ValueToString(theToken.value);

   /*==============================================*/
   /* Set up a router to capture the error output. */
   /*==============================================*/

   EnvAddRouter(theEnv,"cs-error-capture",40,
                FindErrorCapture, PrintErrorCapture,
                NULL, NULL, NULL);

   /*================================*/
   /* Determine if it's a construct. */
   /*================================*/

   if (FindConstruct(theEnv,name))
     {
      ConstructData(theEnv)->CheckSyntaxMode = true;
      rv = (short) ParseConstruct(theEnv,name,"check-syntax");
      GetToken(theEnv,"check-syntax",&theToken);
      ConstructData(theEnv)->CheckSyntaxMode = false;

      if (rv)
        {
         EnvPrintRouter(theEnv,WERROR,"\nERROR:\n");
         PrintInChunks(theEnv,WERROR,GetPPBuffer(theEnv));
         EnvPrintRouter(theEnv,WERROR,"\n");
        }

      DestroyPPBuffer(theEnv);

      CloseStringSource(theEnv,"check-syntax");

      if ((rv != false) || (ParseFunctionData(theEnv)->WarningString != NULL))
        {
         SetErrorCaptureValues(theEnv,returnValue);
         DeactivateErrorCapture(theEnv);
         return true;
        }

      if (theToken.type != STOP)
        {
         SetpValue(returnValue,EnvAddSymbol(theEnv,"EXTRANEOUS-INPUT-AFTER-LAST-PARENTHESIS"));
         DeactivateErrorCapture(theEnv);
         return true;
        }

      mCVSetBoolean(returnValue,false);
      DeactivateErrorCapture(theEnv);
      return false;
     }

   /*=======================*/
   /* Parse the expression. */
   /*=======================*/

   top = Function2Parse(theEnv,"check-syntax",name);
   GetToken(theEnv,"check-syntax",&theToken);
   ClearParsedBindNames(theEnv);
   CloseStringSource(theEnv,"check-syntax");

   if (top == NULL)
     {
      SetErrorCaptureValues(theEnv,returnValue);
      DeactivateErrorCapture(theEnv);
      return true;
     }

   if (theToken.type != STOP)
     {
      mCVSetSymbol(returnValue,"EXTRANEOUS-INPUT-AFTER-LAST-PARENTHESIS");
      DeactivateErrorCapture(theEnv);
      ReturnExpression(theEnv,top);
      return true;
     }

   DeactivateErrorCapture(theEnv);

   ReturnExpression(theEnv,top);
   mCVSetBoolean(returnValue,false);
   return false;
  }

/**************************************************/
/* DeactivateErrorCapture: Deactivates the error  */
/*   capture router and the strings used to store */
/*   the captured information.                    */
/**************************************************/
static void DeactivateErrorCapture(
  Environment *theEnv)
  {   
   if (ParseFunctionData(theEnv)->ErrorString != NULL)
     {
      rm(theEnv,ParseFunctionData(theEnv)->ErrorString,ParseFunctionData(theEnv)->ErrorMaximumPosition);
      ParseFunctionData(theEnv)->ErrorString = NULL;
     }

   if (ParseFunctionData(theEnv)->WarningString != NULL)
     {
      rm(theEnv,ParseFunctionData(theEnv)->WarningString,ParseFunctionData(theEnv)->WarningMaximumPosition);
      ParseFunctionData(theEnv)->WarningString = NULL;
     }

   ParseFunctionData(theEnv)->ErrorCurrentPosition = 0;
   ParseFunctionData(theEnv)->ErrorMaximumPosition = 0;
   ParseFunctionData(theEnv)->WarningCurrentPosition = 0;
   ParseFunctionData(theEnv)->WarningMaximumPosition = 0;

   EnvDeleteRouter(theEnv,"cs-error-capture");
  }

/*******************************************************************/
/* SetErrorCaptureValues: Stores the error/warnings captured when  */
/*   parsing an expression or construct into a multifield value.   */
/*   The first field contains the output sent to the WERROR        */
/*   logical name and the second field contains the output sent    */
/*   to the WWARNING logical name. The symbol FALSE is stored in   */
/*   either position if no output was sent to those logical names. */
/*******************************************************************/
static void SetErrorCaptureValues(
  Environment *theEnv,
  CLIPSValue *returnValue)
  {
   Multifield *theMultifield;

   theMultifield = EnvCreateMultifield(theEnv,2L);

   if (ParseFunctionData(theEnv)->ErrorString != NULL)
     {
      SetMFType(theMultifield,1,STRING);
      SetMFValue(theMultifield,1,EnvAddSymbol(theEnv,ParseFunctionData(theEnv)->ErrorString));
     }
   else
     {
      SetMFType(theMultifield,1,SYMBOL);
      SetMFValue(theMultifield,1,EnvFalseSymbol(theEnv));
     }

   if (ParseFunctionData(theEnv)->WarningString != NULL)
     {
      SetMFType(theMultifield,2,STRING);
      SetMFValue(theMultifield,2,EnvAddSymbol(theEnv,ParseFunctionData(theEnv)->WarningString));
     }
   else
     {
      SetMFType(theMultifield,2,SYMBOL);
      SetMFValue(theMultifield,2,EnvFalseSymbol(theEnv));
     }

   SetpType(returnValue,MULTIFIELD);
   SetpDOBegin(returnValue,1);
   SetpDOEnd(returnValue,2);
   SetpValue(returnValue,theMultifield);
  }

/**********************************/
/* FindErrorCapture: Find routine */
/*   for the check-syntax router. */
/**********************************/
static bool FindErrorCapture(
  Environment *theEnv,
  const char *logicalName)
  {
#if MAC_XCD
#pragma unused(theEnv)
#endif

   if ((strcmp(logicalName,WERROR) == 0) ||
       (strcmp(logicalName,WWARNING) == 0))
     { return true; }

   return false;
  }

/************************************/
/* PrintErrorCapture: Print routine */
/*   for the check-syntax router.   */
/************************************/
static void PrintErrorCapture(
  Environment *theEnv,
  const char *logicalName,
  const char *str)
  {
   if (strcmp(logicalName,WERROR) == 0)
     {
      ParseFunctionData(theEnv)->ErrorString = AppendToString(theEnv,str,ParseFunctionData(theEnv)->ErrorString,
                                   &ParseFunctionData(theEnv)->ErrorCurrentPosition,
                                   &ParseFunctionData(theEnv)->ErrorMaximumPosition);
     }
   else if (strcmp(logicalName,WWARNING) == 0)
     {
      ParseFunctionData(theEnv)->WarningString = AppendToString(theEnv,str,ParseFunctionData(theEnv)->WarningString,
                                     &ParseFunctionData(theEnv)->WarningCurrentPosition,
                                     &ParseFunctionData(theEnv)->WarningMaximumPosition);
     }
  }

#else
/****************************************************/
/* CheckSyntaxFunction: This is the non-functional  */
/*   stub provided for use with a run-time version. */
/****************************************************/
void CheckSyntaxFunction(
  Environment *theEnv,
  UDFContext *context,
  CLIPSValue *returnValue)
  {
   PrintErrorID(theEnv,"PARSEFUN",1,false);
   EnvPrintRouter(theEnv,WERROR,"Function check-syntax does not work in run time modules.\n");
   SetpType(returnValue,SYMBOL);
   SetpValue(returnValue,EnvTrueSymbol(theEnv));
  }

/************************************************/
/* CheckSyntax: This is the non-functional stub */
/*   provided for use with a run-time version.  */
/************************************************/
bool CheckSyntax(
  Environment *theEnv,
  const char *theString,
  CLIPSValue *returnValue)
  {
   PrintErrorID(theEnv,"PARSEFUN",1,false);
   EnvPrintRouter(theEnv,WERROR,"Function check-syntax does not work in run time modules.\n");
   SetpType(returnValue,SYMBOL);
   SetpValue(returnValue,EnvTrueSymbol(theEnv));
   return true;
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */


