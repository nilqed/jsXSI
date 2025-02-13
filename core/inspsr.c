   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  07/30/16             */
   /*                                                     */
   /*               INSTANCE PARSER MODULE                */
   /*******************************************************/

/*************************************************************/
/* Purpose:  Instance Function Parsing Routines              */
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
/*            Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Fixed ParseSlotOverrides memory release issue. */
/*                                                           */
/*            It's now possible to create an instance of a   */
/*            class that's not in scope if the module name   */
/*            is specified.                                  */
/*                                                           */
/*            Added code to keep track of pointers to        */
/*            constructs that are contained externally to    */
/*            to constructs, DanglingConstructs.             */
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
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if OBJECT_SYSTEM

#include <stdio.h>
#include <string.h>

#include "classcom.h"
#include "classfun.h"
#include "classinf.h"
#include "constant.h"
#include "envrnmnt.h"
#include "evaluatn.h"
#include "exprnpsr.h"
#include "moduldef.h"
#include "prntutil.h"
#include "router.h"

#include "inspsr.h"

/* =========================================
   *****************************************
                   CONSTANTS
   =========================================
   ***************************************** */
#define MAKE_TYPE       0
#define INITIALIZE_TYPE 1
#define MODIFY_TYPE     2
#define DUPLICATE_TYPE  3

#define CLASS_RLN          "of"
#define DUPLICATE_NAME_REF "to"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    ReplaceClassNameWithReference(Environment *,EXPRESSION *);

/* =========================================
   *****************************************
          EXTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

#if ! RUN_TIME

/*************************************************************************************
  NAME         : ParseInitializeInstance
  DESCRIPTION  : Parses initialize-instance and make-instance function
                   calls into an EXPRESSION form that
                   can later be evaluated with EvaluateExpression(theEnv,)
  INPUTS       : 1) The address of the top node of the expression
                    containing the initialize-instance function call
                 2) The logical name of the input source
  RETURNS      : The address of the modified expression, or NULL
                    if there is an error
  SIDE EFFECTS : The expression is enhanced to include all
                    aspects of the initialize-instance call
                    (slot-overrides etc.)
                 The "top" expression is deleted on errors.
  NOTES        : This function parses a initialize-instance call into
                 an expression of the following form :

                 (initialize-instance <instance-name> <slot-override>*)
                  where <slot-override> ::= (<slot-name> <expression>+)

                  goes to -->

                  initialize-instance
                      |
                      V
                  <instance or name>-><slot-name>-><dummy-node>...
                                                      |
                                                      V
                                               <value-expression>...

                  (make-instance <instance> of <class> <slot-override>*)
                  goes to -->

                  make-instance
                      |
                      V
                  <instance-name>-><class-name>-><slot-name>-><dummy-node>...
                                                                 |
                                                                 V
                                                          <value-expression>...

                  (make-instance of <class> <slot-override>*)
                  goes to -->

                  make-instance
                      |
                      V
                  (gensym*)-><class-name>-><slot-name>-><dummy-node>...
                                                                 |
                                                                 V
                                                          <value-expression>...

                  (modify-instance <instance> <slot-override>*)
                  goes to -->

                  modify-instance
                      |
                      V
                  <instance or name>-><slot-name>-><dummy-node>...
                                                      |
                                                      V
                                               <value-expression>...

                  (duplicate-instance <instance> [to <new-name>] <slot-override>*)
                  goes to -->

                  duplicate-instance
                      |
                      V
                  <instance or name>-><new-name>-><slot-name>-><dummy-node>...
                                          OR                         |
                                      (gensym*)                      V
                                                           <value-expression>...

 *************************************************************************************/
EXPRESSION *ParseInitializeInstance(
  Environment *theEnv,
  EXPRESSION *top,
  const char *readSource)
  {
   bool error;
   int fcalltype;
   bool readclass;

   if ((top->value == FindFunction(theEnv,"make-instance")) ||
       (top->value == FindFunction(theEnv,"active-make-instance")))
     fcalltype = MAKE_TYPE;
   else if ((top->value == FindFunction(theEnv,"initialize-instance")) ||
            (top->value == FindFunction(theEnv,"active-initialize-instance")))
     fcalltype = INITIALIZE_TYPE;
   else if ((top->value == FindFunction(theEnv,"modify-instance")) ||
            (top->value == FindFunction(theEnv,"active-modify-instance")) ||
            (top->value == FindFunction(theEnv,"message-modify-instance")) ||
            (top->value == FindFunction(theEnv,"active-message-modify-instance")))
     fcalltype = MODIFY_TYPE;
   else
     fcalltype = DUPLICATE_TYPE;
   IncrementIndentDepth(theEnv,3);
   error = false;
   if (top->type == UNKNOWN_VALUE)
     top->type = FCALL;
   else
     SavePPBuffer(theEnv," ");
   top->argList = ArgumentParse(theEnv,readSource,&error);
   if (error)
     goto ParseInitializeInstanceError;
   else if (top->argList == NULL)
     {
      SyntaxErrorMessage(theEnv,"instance");
      goto ParseInitializeInstanceError;
     }
   SavePPBuffer(theEnv," ");

   if (fcalltype == MAKE_TYPE)
     {
      /* ======================================
         Handle the case of anonymous instances
         where the name was not specified
         ====================================== */
      if ((top->argList->type != SYMBOL) ? false :
          (strcmp(ValueToString(top->argList->value),CLASS_RLN) == 0))
        {
         top->argList->nextArg = ArgumentParse(theEnv,readSource,&error);
         if (error == true)
           goto ParseInitializeInstanceError;
         if (top->argList->nextArg == NULL)
           {
            SyntaxErrorMessage(theEnv,"instance class");
            goto ParseInitializeInstanceError;
           }
         if ((top->argList->nextArg->type != SYMBOL) ? true :
             (strcmp(ValueToString(top->argList->nextArg->value),CLASS_RLN) != 0))
           {
            top->argList->type = FCALL;
            top->argList->value = FindFunction(theEnv,"gensym*");
            readclass = false;
           }
         else
           readclass = true;
        }
      else
        {
         GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
         if ((GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL) ? true :
             (strcmp(CLASS_RLN,DOToString(DefclassData(theEnv)->ObjectParseToken)) != 0))
           {
            SyntaxErrorMessage(theEnv,"make-instance");
            goto ParseInitializeInstanceError;
           }
         SavePPBuffer(theEnv," ");
         readclass = true;
        }
      if (readclass)
        {
         top->argList->nextArg = ArgumentParse(theEnv,readSource,&error);
         if (error)
           goto ParseInitializeInstanceError;
         if (top->argList->nextArg == NULL)
           {
            SyntaxErrorMessage(theEnv,"instance class");
            goto ParseInitializeInstanceError;
           }
        }

      /* ==============================================
         If the class name is a constant, go ahead and
         look it up now and replace it with the pointer
         ============================================== */
      if (ReplaceClassNameWithReference(theEnv,top->argList->nextArg) == false)
        goto ParseInitializeInstanceError;

      PPCRAndIndent(theEnv);
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      top->argList->nextArg->nextArg =
                  ParseSlotOverrides(theEnv,readSource,&error);
     }
   else
     {
      PPCRAndIndent(theEnv);
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      if (fcalltype == DUPLICATE_TYPE)
        {
         if ((DefclassData(theEnv)->ObjectParseToken.type != SYMBOL) ? false :
             (strcmp(DOToString(DefclassData(theEnv)->ObjectParseToken),DUPLICATE_NAME_REF) == 0))
           {
            PPBackup(theEnv);
            PPBackup(theEnv);
            SavePPBuffer(theEnv,DefclassData(theEnv)->ObjectParseToken.printForm);
            SavePPBuffer(theEnv," ");
            top->argList->nextArg = ArgumentParse(theEnv,readSource,&error);
            if (error)
              goto ParseInitializeInstanceError;
            if (top->argList->nextArg == NULL)
              {
               SyntaxErrorMessage(theEnv,"instance name");
               goto ParseInitializeInstanceError;
              }
            PPCRAndIndent(theEnv);
            GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
           }
         else
           top->argList->nextArg = GenConstant(theEnv,FCALL,FindFunction(theEnv,"gensym*"));
         top->argList->nextArg->nextArg = ParseSlotOverrides(theEnv,readSource,&error);
        }
      else
        top->argList->nextArg = ParseSlotOverrides(theEnv,readSource,&error);
     }
   if (error)
      goto ParseInitializeInstanceError;
   if (GetType(DefclassData(theEnv)->ObjectParseToken) != RPAREN)
     {
      SyntaxErrorMessage(theEnv,"slot-override");
      goto ParseInitializeInstanceError;
     }
   DecrementIndentDepth(theEnv,3);
   return(top);

ParseInitializeInstanceError:
   EnvSetEvaluationError(theEnv,true);
   ReturnExpression(theEnv,top);
   DecrementIndentDepth(theEnv,3);
   return NULL;
  }

/********************************************************************************
  NAME         : ParseSlotOverrides
  DESCRIPTION  : Forms expressions for slot-overrides
  INPUTS       : 1) The logical name of the input
                 2) Caller's buffer for error flkag
  RETURNS      : Address override expressions, NULL
                   if none or error.
  SIDE EFFECTS : Slot-expression built
                 Caller's error flag set
  NOTES        : <slot-override> ::= (<slot-name> <value>*)*

                 goes to

                 <slot-name> --> <dummy-node> --> <slot-name> --> <dummy-node>...
                                       |
                                       V
                               <value-expression> --> <value-expression> --> ...

                 Assumes first token has already been scanned
 ********************************************************************************/
EXPRESSION *ParseSlotOverrides(
  Environment *theEnv,
  const char *readSource,
  bool *error)
  {
   EXPRESSION *top = NULL,*bot = NULL,*theExp;
   EXPRESSION *theExpNext;

   while (GetType(DefclassData(theEnv)->ObjectParseToken) == LPAREN)
     {
      *error = false;
      theExp = ArgumentParse(theEnv,readSource,error);
      if (*error == true)
        {
         ReturnExpression(theEnv,top);
         return NULL;
        }
      else if (theExp == NULL)
        {
         SyntaxErrorMessage(theEnv,"slot-override");
         *error = true;
         ReturnExpression(theEnv,top);
         EnvSetEvaluationError(theEnv,true);
         return NULL;
        }
      theExpNext = GenConstant(theEnv,SYMBOL,EnvTrueSymbol(theEnv));
      if (CollectArguments(theEnv,theExpNext,readSource) == NULL)
        {
         *error = true;
         ReturnExpression(theEnv,top);
         ReturnExpression(theEnv,theExp);
         return NULL;
        }
      theExp->nextArg = theExpNext;
      if (top == NULL)
        top = theExp;
      else
        bot->nextArg = theExp;
      bot = theExp->nextArg;
      PPCRAndIndent(theEnv);
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
     }
   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,DefclassData(theEnv)->ObjectParseToken.printForm);
   return(top);
  }

#endif

/****************************************************************************
  NAME         : ParseSimpleInstance
  DESCRIPTION  : Parses instances from file for load-instances
                   into an EXPRESSION forms that
                   can later be evaluated with EvaluateExpression(theEnv,)
  INPUTS       : 1) The address of the top node of the expression
                    containing the make-instance function call
                 2) The logical name of the input source
  RETURNS      : The address of the modified expression, or NULL
                    if there is an error
  SIDE EFFECTS : The expression is enhanced to include all
                    aspects of the make-instance call
                    (slot-overrides etc.)
                 The "top" expression is deleted on errors.
  NOTES        : The name, class, values etc. must be constants.

                 This function parses a make-instance call into
                 an expression of the following form :

                  (make-instance <instance> of <class> <slot-override>*)
                  where <slot-override> ::= (<slot-name> <expression>+)

                  goes to -->

                  make-instance
                      |
                      V
                  <instance-name>-><class-name>-><slot-name>-><dummy-node>...
                                                                 |
                                                                 V
                                                          <value-expression>...

 ****************************************************************************/
EXPRESSION *ParseSimpleInstance(
  Environment *theEnv,
  EXPRESSION *top,
  const char *readSource)
  {
   EXPRESSION *theExp,*vals = NULL,*vbot,*tval;
   unsigned short type;

   GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   if ((GetType(DefclassData(theEnv)->ObjectParseToken) != INSTANCE_NAME) &&
       (GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL))
     goto MakeInstanceError;

   if ((GetType(DefclassData(theEnv)->ObjectParseToken) == SYMBOL) &&
       (strcmp(CLASS_RLN,DOToString(DefclassData(theEnv)->ObjectParseToken)) == 0))
     {
      top->argList = GenConstant(theEnv,FCALL,
                                 (void *) FindFunction(theEnv,"gensym*"));
     }
   else
     {
      top->argList = GenConstant(theEnv,INSTANCE_NAME,
                                 (void *) GetValue(DefclassData(theEnv)->ObjectParseToken));
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      if ((GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL) ? true :
          (strcmp(CLASS_RLN,DOToString(DefclassData(theEnv)->ObjectParseToken)) != 0))
        goto MakeInstanceError;
     }

   GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   if (GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL)
     goto MakeInstanceError;
   top->argList->nextArg =
        GenConstant(theEnv,SYMBOL,GetValue(DefclassData(theEnv)->ObjectParseToken));
   theExp = top->argList->nextArg;
   if (ReplaceClassNameWithReference(theEnv,theExp) == false)
     goto MakeInstanceError;
   GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
   while (GetType(DefclassData(theEnv)->ObjectParseToken) == LPAREN)
     {
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      if (GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL)
        goto SlotOverrideError;
      theExp->nextArg = GenConstant(theEnv,SYMBOL,GetValue(DefclassData(theEnv)->ObjectParseToken));
      theExp->nextArg->nextArg = GenConstant(theEnv,SYMBOL,EnvTrueSymbol(theEnv));
      theExp = theExp->nextArg->nextArg;
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      vbot = NULL;
      while (GetType(DefclassData(theEnv)->ObjectParseToken) != RPAREN)
        {
         type = GetType(DefclassData(theEnv)->ObjectParseToken);
         if (type == LPAREN)
           {
            GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
            if ((GetType(DefclassData(theEnv)->ObjectParseToken) != SYMBOL) ? true :
                (strcmp(ValueToString(DefclassData(theEnv)->ObjectParseToken.value),"create$") != 0))
              goto SlotOverrideError;
            GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
            if (GetType(DefclassData(theEnv)->ObjectParseToken) != RPAREN)
              goto SlotOverrideError;
            tval = GenConstant(theEnv,FCALL,FindFunction(theEnv,"create$"));
           }
         else
           {
            if ((type != SYMBOL) && (type != STRING) &&
                (type != FLOAT) && (type != INTEGER) && (type != INSTANCE_NAME))
              goto SlotOverrideError;
            tval = GenConstant(theEnv,type,GetValue(DefclassData(theEnv)->ObjectParseToken));
           }
         if (vals == NULL)
           vals = tval;
         else
           vbot->nextArg = tval;
         vbot = tval;
         GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
        }
      theExp->argList = vals;
      GetToken(theEnv,readSource,&DefclassData(theEnv)->ObjectParseToken);
      vals = NULL;
     }
   if (GetType(DefclassData(theEnv)->ObjectParseToken) != RPAREN)
     goto SlotOverrideError;
   return(top);

MakeInstanceError:
   SyntaxErrorMessage(theEnv,"make-instance");
   EnvSetEvaluationError(theEnv,true);
   ReturnExpression(theEnv,top);
   return NULL;

SlotOverrideError:
   SyntaxErrorMessage(theEnv,"slot-override");
   EnvSetEvaluationError(theEnv,true);
   ReturnExpression(theEnv,top);
   ReturnExpression(theEnv,vals);
   return NULL;
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/***************************************************
  NAME         : ReplaceClassNameWithReference
  DESCRIPTION  : In parsing a make instance call,
                 this function replaces a constant
                 class name with an actual pointer
                 to the class
  INPUTS       : The expression
  RETURNS      : True if all OK, false
                 if class cannot be found
  SIDE EFFECTS : The expression type and value are
                 modified if class is found
  NOTES        : Searches current nd imported
                 modules for reference
  CHANGES      : It's now possible to create an instance of a
                 class that's not in scope if the module name
                 is specified.
 ***************************************************/
static bool ReplaceClassNameWithReference(
  Environment *theEnv,
  EXPRESSION *theExp)
  {
   const char *theClassName;
   void *theDefclass;

   if (theExp->type == SYMBOL)
     {
      theClassName = ValueToString(theExp->value);
      //theDefclass = (void *) LookupDefclassInScope(theEnv,theClassName);
      theDefclass = (void *) LookupDefclassByMdlOrScope(theEnv,theClassName); // Module or scope is now allowed
      if (theDefclass == NULL)
        {
         CantFindItemErrorMessage(theEnv,"class",theClassName);
         return false;
        }
      if (EnvClassAbstractP(theEnv,theDefclass))
        {
         PrintErrorID(theEnv,"INSMNGR",3,false);
         EnvPrintRouter(theEnv,WERROR,"Cannot create instances of abstract class ");
         EnvPrintRouter(theEnv,WERROR,theClassName);
         EnvPrintRouter(theEnv,WERROR,".\n");
         return false;
        }
      theExp->type = DEFCLASS_PTR;
      theExp->value = theDefclass;
      
#if (! RUN_TIME) && (! BLOAD_ONLY)
      if (! ConstructData(theEnv)->ParsingConstruct)
        { ConstructData(theEnv)->DanglingConstructs++; }
#endif
     }
   return true;
  }

#endif



