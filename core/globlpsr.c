   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/25/16             */
   /*                                                     */
   /*              DEFGLOBAL PARSER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose: Parses the defglobal construct.                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            Made the construct redefinition message more   */
/*            prominent.                                     */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW and       */
/*            MAC_MCW).                                      */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Moved WatchGlobals global to defglobalData.    */
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
/*************************************************************/

#include "setup.h"

#if DEFGLOBAL_CONSTRUCT

#include <string.h>

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
#include "bload.h"
#endif
#include "constrct.h"
#include "cstrcpsr.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "exprnpsr.h"
#include "globlbsc.h"
#include "globldef.h"
#include "memalloc.h"
#include "modulpsr.h"
#include "modulutl.h"
#include "multifld.h"
#include "pprint.h"
#include "router.h"
#include "scanner.h"
#include "watch.h"

#include "globlpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

#if (! RUN_TIME) && (! BLOAD_ONLY)
   static bool                    GetVariableDefinition(Environment *,const char *,bool *,bool,struct token *);
   static void                    AddDefglobal(Environment *,SYMBOL_HN *,CLIPSValue *,struct expr *);
#endif

/*********************************************************************/
/* ParseDefglobal: Coordinates all actions necessary for the parsing */
/*   and creation of a defglobal into the current environment.       */
/*********************************************************************/
bool ParseDefglobal(
  Environment *theEnv,
  const char *readSource)
  {
   bool defglobalError = false;
#if (! RUN_TIME) && (! BLOAD_ONLY)

   struct token theToken;
   bool tokenRead = true;
   Defmodule *theModule;

   /*=====================================*/
   /* Pretty print buffer initialization. */
   /*=====================================*/

   SetPPBufferStatus(theEnv,true);
   FlushPPBuffer(theEnv);
   SetIndentDepth(theEnv,3);
   SavePPBuffer(theEnv,"(defglobal ");

   /*=================================================*/
   /* Individual defglobal constructs can't be parsed */
   /* while a binary load is in effect.               */
   /*=================================================*/

#if BLOAD || BLOAD_ONLY || BLOAD_AND_BSAVE
   if ((Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CheckSyntaxMode))
     {
      CannotLoadWithBloadMessage(theEnv,"defglobal");
      return true;
     }
#endif

   /*===========================*/
   /* Look for the module name. */
   /*===========================*/

   GetToken(theEnv,readSource,&theToken);
   if (theToken.type == SYMBOL)
     {
      /*=================================================*/
      /* The optional module name can't contain a module */
      /* separator like other constructs. For example,   */
      /* (defrule X::foo is OK for rules, but the right  */
      /* syntax for defglobals is (defglobal X ?*foo*.   */
      /*=================================================*/

      tokenRead = false;
      if (FindModuleSeparator(ValueToString(theToken.value)))
        {
         SyntaxErrorMessage(theEnv,"defglobal");
         return true;
        }

      /*=================================*/
      /* Determine if the module exists. */
      /*=================================*/

      theModule = EnvFindDefmodule(theEnv,ValueToString(theToken.value));
      if (theModule == NULL)
        {
         CantFindItemErrorMessage(theEnv,"defmodule",ValueToString(theToken.value));
         return true;
        }

      /*=========================================*/
      /* If the module name was OK, then set the */
      /* current module to the specified module. */
      /*=========================================*/

      SavePPBuffer(theEnv," ");
      EnvSetCurrentModule(theEnv,theModule);
     }

   /*===========================================*/
   /* If the module name wasn't specified, then */
   /* use the current module's name in the      */
   /* defglobal's pretty print representation.  */
   /*===========================================*/

   else
     {
      PPBackup(theEnv);
      SavePPBuffer(theEnv,EnvGetDefmoduleName(theEnv,EnvGetCurrentModule(theEnv)));
      SavePPBuffer(theEnv," ");
      SavePPBuffer(theEnv,theToken.printForm);
     }

   /*======================*/
   /* Parse the variables. */
   /*======================*/

   while (GetVariableDefinition(theEnv,readSource,&defglobalError,tokenRead,&theToken))
     {
      tokenRead = false;

      FlushPPBuffer(theEnv);
      SavePPBuffer(theEnv,"(defglobal ");
      SavePPBuffer(theEnv,EnvGetDefmoduleName(theEnv,EnvGetCurrentModule(theEnv)));
      SavePPBuffer(theEnv," ");
     }

#endif

   /*==================================*/
   /* Return the parsing error status. */
   /*==================================*/

   return(defglobalError);
  }

#if (! RUN_TIME) && (! BLOAD_ONLY)

/***************************************************************/
/* GetVariableDefinition: Parses and evaluates a single global */
/*   variable in a defglobal construct. Returns true if the    */
/*   variable was successfully parsed and false if a right     */
/*   parenthesis is encountered (signifying the end of the     */
/*   defglobal construct) or an error occurs. The error status */
/*   flag is also set if an error occurs.                      */
/***************************************************************/
static bool GetVariableDefinition(
  Environment *theEnv,
  const char *readSource,
  bool *defglobalError,
  bool tokenRead,
  struct token *theToken)
  {
   SYMBOL_HN *variableName;
   struct expr *assignPtr;
   CLIPSValue assignValue;

   /*========================================*/
   /* Get next token, which should either be */
   /* a closing parenthesis or a variable.   */
   /*========================================*/

   if (! tokenRead) GetToken(theEnv,readSource,theToken);
   if (theToken->type == RPAREN) return false;

   if (theToken->type == SF_VARIABLE)
     {
      SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }
   else if (theToken->type != GBL_VARIABLE)
     {
      SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }

   variableName = (SYMBOL_HN *) theToken->value;

   SavePPBuffer(theEnv," ");

   /*================================*/
   /* Print out compilation message. */
   /*================================*/

#if DEBUGGING_FUNCTIONS
   if ((EnvGetWatchItem(theEnv,"compilations") == true) && GetPrintWhileLoading(theEnv))
     {
      const char *outRouter = WDIALOG;
      if (QFindDefglobal(theEnv,variableName) != NULL) 
        {
         outRouter = WWARNING;
         PrintWarningID(theEnv,"CSTRCPSR",1,true);
         EnvPrintRouter(theEnv,outRouter,"Redefining defglobal: ");
        }
      else EnvPrintRouter(theEnv,outRouter,"Defining defglobal: ");
      EnvPrintRouter(theEnv,outRouter,ValueToString(variableName));
      EnvPrintRouter(theEnv,outRouter,"\n");
     }
   else
#endif
     { if (GetPrintWhileLoading(theEnv)) EnvPrintRouter(theEnv,WDIALOG,":"); }

   /*==================================================================*/
   /* Check for import/export conflicts from the construct definition. */
   /*==================================================================*/

#if DEFMODULE_CONSTRUCT
   if (FindImportExportConflict(theEnv,"defglobal",EnvGetCurrentModule(theEnv),ValueToString(variableName)))
     {
      ImportExportConflictMessage(theEnv,"defglobal",ValueToString(variableName),NULL,NULL);
      *defglobalError = true;
      return false;
     }
#endif

   /*==============================*/
   /* The next token must be an =. */
   /*==============================*/

   GetToken(theEnv,readSource,theToken);
   if (strcmp(theToken->printForm,"=") != 0)
     {
      SyntaxErrorMessage(theEnv,"defglobal");
      *defglobalError = true;
      return false;
     }

   SavePPBuffer(theEnv," ");

   /*======================================================*/
   /* Parse the expression to be assigned to the variable. */
   /*======================================================*/

   assignPtr = ParseAtomOrExpression(theEnv,readSource,NULL);
   if (assignPtr == NULL)
     {
      *defglobalError = true;
      return false;
     }

   /*==========================*/
   /* Evaluate the expression. */
   /*==========================*/

   if (! ConstructData(theEnv)->CheckSyntaxMode)
     {
      EnvSetEvaluationError(theEnv,false);
      if (EvaluateExpression(theEnv,assignPtr,&assignValue))
        {
         ReturnExpression(theEnv,assignPtr);
         *defglobalError = true;
         return false;
        }
     }
   else
     { ReturnExpression(theEnv,assignPtr); }

   SavePPBuffer(theEnv,")");

   /*======================================*/
   /* Add the variable to the global list. */
   /*======================================*/

   if (! ConstructData(theEnv)->CheckSyntaxMode)
     { AddDefglobal(theEnv,variableName,&assignValue,assignPtr); }

   /*==================================================*/
   /* Return true to indicate that the global variable */
   /* definition was successfully parsed.              */
   /*==================================================*/

   return true;
  }

/*********************************************************/
/* AddDefglobal: Adds a defglobal to the current module. */
/*********************************************************/
static void AddDefglobal(
  Environment *theEnv,
  SYMBOL_HN *name,
  CLIPSValue *vPtr,
  struct expr *ePtr)
  {
   Defglobal *defglobalPtr;
   bool newGlobal = false;
#if DEBUGGING_FUNCTIONS
   bool globalHadWatch = false;
#endif

   /*========================================================*/
   /* If the defglobal is already defined, then use the old  */
   /* data structure and substitute new values. If it hasn't */
   /* been defined, then create a new data structure.        */
   /*========================================================*/

   defglobalPtr = QFindDefglobal(theEnv,name);
   if (defglobalPtr == NULL)
     {
      newGlobal = true;
      defglobalPtr = get_struct(theEnv,defglobal);
     }
   else
     {
      DeinstallConstructHeader(theEnv,&defglobalPtr->header);
#if DEBUGGING_FUNCTIONS
      globalHadWatch = defglobalPtr->watch;
#endif
     }

   /*===========================================*/
   /* Remove the old values from the defglobal. */
   /*===========================================*/

   if (newGlobal == false)
     {
      ValueDeinstall(theEnv,&defglobalPtr->current);
      if (defglobalPtr->current.type == MULTIFIELD)
        { ReturnMultifield(theEnv,(struct multifield *) defglobalPtr->current.value); }

      RemoveHashedExpression(theEnv,defglobalPtr->initial);
     }

   /*=======================================*/
   /* Copy the new values to the defglobal. */
   /*=======================================*/

   defglobalPtr->current.type = vPtr->type;
   if (vPtr->type != MULTIFIELD) defglobalPtr->current.value = vPtr->value;
   else DuplicateMultifield(theEnv,&defglobalPtr->current,vPtr);
   ValueInstall(theEnv,&defglobalPtr->current);

   defglobalPtr->initial = AddHashedExpression(theEnv,ePtr);
   ReturnExpression(theEnv,ePtr);
   DefglobalData(theEnv)->ChangeToGlobals = true;

   /*=================================*/
   /* Restore the old watch value to  */
   /* the defglobal if redefined.     */
   /*=================================*/

#if DEBUGGING_FUNCTIONS
   defglobalPtr->watch = globalHadWatch ? true : DefglobalData(theEnv)->WatchGlobals;
#endif

   /*======================================*/
   /* Save the name and pretty print form. */
   /*======================================*/

   defglobalPtr->header.name = name;
   defglobalPtr->header.usrData = NULL;
   IncrementSymbolCount(name);

   SavePPBuffer(theEnv,"\n");
   if (EnvGetConserveMemory(theEnv) == true)
     { defglobalPtr->header.ppForm = NULL; }
   else
     { defglobalPtr->header.ppForm = CopyPPBuffer(theEnv); }

   defglobalPtr->inScope = true;

   /*=============================================*/
   /* If the defglobal was redefined, we're done. */
   /*=============================================*/

   if (newGlobal == false) return;

   /*===================================*/
   /* Copy the defglobal variable name. */
   /*===================================*/

   defglobalPtr->busyCount = 0;
   defglobalPtr->header.whichModule = (struct defmoduleItemHeader *)
                               GetModuleItem(theEnv,NULL,FindModuleItem(theEnv,"defglobal")->moduleIndex);

   /*=============================================*/
   /* Add the defglobal to the list of defglobals */
   /* for the current module.                     */
   /*=============================================*/

   AddConstructToModule(&defglobalPtr->header);
  }

/*****************************************************************/
/* ReplaceGlobalVariable: Replaces a global variable found in an */
/*   expression with the appropriate primitive data type which   */
/*   can later be used to retrieve the global variable's value.  */
/*****************************************************************/
bool ReplaceGlobalVariable(
  Environment *theEnv,
  struct expr *ePtr)
  {
   Defglobal *theGlobal;
   int count;

   /*=================================*/
   /* Search for the global variable. */
   /*=================================*/

   theGlobal = (Defglobal *)
               FindImportedConstruct(theEnv,"defglobal",NULL,ValueToString(ePtr->value),
                                     &count,true,NULL);

   /*=============================================*/
   /* If it wasn't found, print an error message. */
   /*=============================================*/

   if (theGlobal == NULL)
     {
      GlobalReferenceErrorMessage(theEnv,ValueToString(ePtr->value));
      return false;
     }

   /*========================================================*/
   /* The current implementation of the defmodules shouldn't */
   /* allow a construct to be defined which would cause an   */
   /* ambiguous reference, but we'll check for it anyway.    */
   /*========================================================*/

   if (count > 1)
     {
      AmbiguousReferenceErrorMessage(theEnv,"defglobal",ValueToString(ePtr->value));
      return false;
     }

   /*==============================================*/
   /* Replace the symbolic reference of the global */
   /* variable with a direct pointer reference.    */
   /*==============================================*/

   ePtr->type = DEFGLOBAL_PTR;
   ePtr->value = theGlobal;

   return true;
  }

/*****************************************************************/
/* GlobalReferenceErrorMessage: Prints an error message when a   */
/*   symbolic reference to a global variable cannot be resolved. */
/*****************************************************************/
void GlobalReferenceErrorMessage(
  Environment *theEnv,
  const char *variableName)
  {
   PrintErrorID(theEnv,"GLOBLPSR",1,true);
   EnvPrintRouter(theEnv,WERROR,"\nGlobal variable ?*");
   EnvPrintRouter(theEnv,WERROR,variableName);
   EnvPrintRouter(theEnv,WERROR,"* was referenced, but is not defined.\n");
  }

#endif /* (! RUN_TIME) && (! BLOAD_ONLY) */

#endif /* DEFGLOBAL_CONSTRUCT */



