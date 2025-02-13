   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.50  07/30/16             */
   /*                                                     */
   /*                                                     */
   /*******************************************************/

/*************************************************************/
/* Purpose: Deffunction Parsing Routines                     */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Gary D. Riley                                        */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*            If the last construct in a loaded file is a    */
/*            deffunction or defmethod with no closing right */
/*            parenthesis, an error should be issued, but is */
/*            not. DR0872                                    */
/*                                                           */
/*            Added pragmas to prevent unused variable       */
/*            warnings.                                      */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            ENVIRONMENT_API_ONLY no longer supported.      */
/*                                                           */
/*            GetConstructNameAndComment API change.         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*            Changed find construct functionality so that   */
/*            imported modules are search when locating a    */
/*            named construct.                               */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*      6.50: Fact ?var:slot references in deffunctions.     */
/*                                                           */
/*************************************************************/

/* =========================================
   *****************************************
               EXTERNAL DEFINITIONS
   =========================================
   ***************************************** */
#include "setup.h"

#if DEFFUNCTION_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME)

#if BLOAD || BLOAD_AND_BSAVE
#include "bload.h"
#endif

#if DEFRULE_CONSTRUCT
#include "network.h"
#endif

#if DEFGENERIC_CONSTRUCT
#include "genrccom.h"
#endif

#if DEFTEMPLATE_CONSTRUCT
#include "factgen.h"
#endif

#include "constant.h"
#include "cstrcpsr.h"
#include "constrct.h"
#include "dffnxfun.h"
#include "envrnmnt.h"
#include "expressn.h"
#include "exprnpsr.h"
#include "memalloc.h"
#include "prccode.h"
#include "router.h"
#include "scanner.h"
#include "symbol.h"

#include "dffnxpsr.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    ValidDeffunctionName(Environment *,const char *);
   static Deffunction            *AddDeffunction(Environment *,SYMBOL_HN *,EXPRESSION *,int,int,int,bool);

/***************************************************************************
  NAME         : ParseDeffunction
  DESCRIPTION  : Parses the deffunction construct
  INPUTS       : The input logical name
  RETURNS      : False if successful parse, true otherwise
  SIDE EFFECTS : Creates valid deffunction definition
  NOTES        : H/L Syntax :
                 (deffunction <name> [<comment>]
                    (<single-field-varible>* [<multifield-variable>])
                    <action>*)
 ***************************************************************************/
bool ParseDeffunction(
  Environment *theEnv,
  const char *readSource)
  {
   SYMBOL_HN *deffunctionName;
   EXPRESSION *actions;
   EXPRESSION *parameterList;
   SYMBOL_HN *wildcard;
   int min,max,lvars;
   bool deffunctionError = false;
   bool overwrite = false;
   short owMin = 0, owMax = 0;
   Deffunction *dptr;

   SetPPBufferStatus(theEnv,true);

   FlushPPBuffer(theEnv);
   SetIndentDepth(theEnv,3);
   SavePPBuffer(theEnv,"(deffunction ");

#if BLOAD || BLOAD_AND_BSAVE
   if ((Bloaded(theEnv) == true) && (! ConstructData(theEnv)->CheckSyntaxMode))
     {
      CannotLoadWithBloadMessage(theEnv,"deffunctions");
      return true;
     }
#endif

   /*=======================================================*/
   /* Parse the name and comment fields of the deffunction. */
   /*=======================================================*/

   deffunctionName = GetConstructNameAndComment(theEnv,readSource,&DeffunctionData(theEnv)->DFInputToken,"deffunction",
                                                (FindConstructFunction *) EnvFindDeffunctionInModule,
                                                NULL,
                                                "!",true,true,true,false);

   if (deffunctionName == NULL)
     { return true; }

   if (ValidDeffunctionName(theEnv,ValueToString(deffunctionName)) == false)
     { return true; }

   /*==========================*/
   /* Parse the argument list. */
   /*==========================*/
   
   parameterList = ParseProcParameters(theEnv,readSource,&DeffunctionData(theEnv)->DFInputToken,
                                       NULL,&wildcard,&min,&max,&deffunctionError,NULL);
   if (deffunctionError)
     { return true; }

   /*===================================================================*/
   /* Go ahead and add the deffunction so it can be recursively called. */
   /*===================================================================*/

   if (ConstructData(theEnv)->CheckSyntaxMode)
     {
      dptr = EnvFindDeffunctionInModule(theEnv,ValueToString(deffunctionName));
      if (dptr == NULL)
        { dptr = AddDeffunction(theEnv,deffunctionName,NULL,min,max,0,true); }
      else
        {
         overwrite = true;
         owMin = (short) dptr->minNumberOfParameters;
         owMax = (short) dptr->maxNumberOfParameters;
         dptr->minNumberOfParameters = min;
         dptr->maxNumberOfParameters = max;
        }
     }
   else
     { dptr = AddDeffunction(theEnv,deffunctionName,NULL,min,max,0,true); }

   if (dptr == NULL)
     {
      ReturnExpression(theEnv,parameterList);
      return true;
     }

   /*==================================================*/
   /* Parse the actions contained within the function. */
   /*==================================================*/

   PPCRAndIndent(theEnv);

   ExpressionData(theEnv)->ReturnContext = true;
   actions = ParseProcActions(theEnv,"deffunction",readSource,
                              &DeffunctionData(theEnv)->DFInputToken,parameterList,wildcard,
#if DEFTEMPLATE_CONSTRUCT
                              FactSlotReferenceVar, // variable parse function
#else
                              NULL,                 // variable parse function
#endif
                              NULL,                 // bind handler function
                              &lvars,               // local var count
                              NULL);                // special user data buffer

   /*=============================================================*/
   /* Check for the closing right parenthesis of the deffunction. */
   /*=============================================================*/

   if ((DeffunctionData(theEnv)->DFInputToken.type != RPAREN) && /* DR0872 */
       (actions != NULL))
     {
      SyntaxErrorMessage(theEnv,"deffunction");
      
      ReturnExpression(theEnv,parameterList);
      ReturnPackedExpression(theEnv,actions);

      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }

      if ((dptr->busy == 0) && (! overwrite))
        {
         RemoveConstructFromModule(theEnv,(struct constructHeader *) dptr);
         RemoveDeffunction(theEnv,dptr);
        }

      return true;
     }

   if (actions == NULL)
     {
      ReturnExpression(theEnv,parameterList);
      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }

      if ((dptr->busy == 0) && (! overwrite))
        {
         RemoveConstructFromModule(theEnv,(struct constructHeader *) dptr);
         RemoveDeffunction(theEnv,dptr);
        }
      return true;
     }

   /*==============================================*/
   /* If we're only checking syntax, don't add the */
   /* successfully parsed deffunction to the KB.   */
   /*==============================================*/

   if (ConstructData(theEnv)->CheckSyntaxMode)
     {
      ReturnExpression(theEnv,parameterList);
      ReturnPackedExpression(theEnv,actions);
      if (overwrite)
        {
         dptr->minNumberOfParameters = owMin;
         dptr->maxNumberOfParameters = owMax;
        }
      else
        {
         RemoveConstructFromModule(theEnv,(struct constructHeader *) dptr);
         RemoveDeffunction(theEnv,dptr);
        }
      return false;
     }

   /*=============================*/
   /* Reformat the closing token. */
   /*=============================*/

   PPBackup(theEnv);
   PPBackup(theEnv);
   SavePPBuffer(theEnv,DeffunctionData(theEnv)->DFInputToken.printForm);
   SavePPBuffer(theEnv,"\n");

   /*======================*/
   /* Add the deffunction. */
   /*======================*/

   AddDeffunction(theEnv,deffunctionName,actions,min,max,lvars,false);

   ReturnExpression(theEnv,parameterList);

   return(deffunctionError);
  }

/* =========================================
   *****************************************
          INTERNALLY VISIBLE FUNCTIONS
   =========================================
   ***************************************** */

/************************************************************
  NAME         : ValidDeffunctionName
  DESCRIPTION  : Determines if a new deffunction of the given
                 name can be defined in the current module
  INPUTS       : The new deffunction name
  RETURNS      : True if OK, false otherwise
  SIDE EFFECTS : Error message printed if not OK
  NOTES        : GetConstructNameAndComment() (called before
                 this function) ensures that the deffunction
                 name does not conflict with one from
                 another module
 ************************************************************/
static bool ValidDeffunctionName(
  Environment *theEnv,
  const char *theDeffunctionName)
  {
   Deffunction *theDeffunction;
#if DEFGENERIC_CONSTRUCT
   Defmodule *theModule;
   Defgeneric *theDefgeneric;
#endif

   /*==============================================*/
   /* A deffunction cannot be named the same as a  */
   /* construct type, e.g, defclass, defrule, etc. */
   /*==============================================*/
   
   if (FindConstruct(theEnv,theDeffunctionName) != NULL)
     {
      PrintErrorID(theEnv,"DFFNXPSR",1,false);
      EnvPrintRouter(theEnv,WERROR,"Deffunctions are not allowed to replace constructs.\n");
      return false;
     }

   /*========================================*/
   /* A deffunction cannot be named the same */
   /* as a pre-defined system function, e.g, */
   /* watch, list-defrules, etc.             */
   /*========================================*/
   
   if (FindFunction(theEnv,theDeffunctionName) != NULL)
     {
      PrintErrorID(theEnv,"DFFNXPSR",2,false);
      EnvPrintRouter(theEnv,WERROR,"Deffunctions are not allowed to replace external functions.\n");
      return false;
     }

#if DEFGENERIC_CONSTRUCT

   /*===========================================*/
   /* A deffunction cannot be named the same as */
   /* a generic function (either in this module */
   /* or imported from another).                */
   /*===========================================*/
   
   theDefgeneric = LookupDefgenericInScope(theEnv,theDeffunctionName);
     
   if (theDefgeneric != NULL)
     {
      theModule = GetConstructModuleItem(&theDefgeneric->header)->theModule;
      if (theModule != EnvGetCurrentModule(theEnv))
        {
         PrintErrorID(theEnv,"DFFNXPSR",5,false);
         EnvPrintRouter(theEnv,WERROR,"Defgeneric ");
         EnvPrintRouter(theEnv,WERROR,EnvGetDefgenericName(theEnv,theDefgeneric));
         EnvPrintRouter(theEnv,WERROR," imported from module ");
         EnvPrintRouter(theEnv,WERROR,EnvGetDefmoduleName(theEnv,theModule));
         EnvPrintRouter(theEnv,WERROR," conflicts with this deffunction.\n");
         return false;
        }
      else
        {
         PrintErrorID(theEnv,"DFFNXPSR",3,false);
         EnvPrintRouter(theEnv,WERROR,"Deffunctions are not allowed to replace generic functions.\n");
        }
      return false;
     }
#endif

   theDeffunction = EnvFindDeffunctionInModule(theEnv,theDeffunctionName);
   if (theDeffunction != NULL)
     {
      /*=============================================*/
      /* And a deffunction in the current module can */
      /* only be redefined if it is not executing.   */
      /*=============================================*/
      
      if (theDeffunction->executing)
        {
         PrintErrorID(theEnv,"DFNXPSR",4,false);
         EnvPrintRouter(theEnv,WERROR,"Deffunction ");
         EnvPrintRouter(theEnv,WERROR,EnvGetDeffunctionName(theEnv,theDeffunction));
         EnvPrintRouter(theEnv,WERROR," may not be redefined while it is executing.\n");
         return false;
        }
     }
   return true;
  }

/****************************************************
  NAME         : AddDeffunction
  DESCRIPTION  : Adds a deffunction to the list of
                 deffunctions
  INPUTS       : 1) The symbolic name
                 2) The action expressions
                 3) The minimum number of arguments
                 4) The maximum number of arguments
                    (can be -1)
                 5) The number of local variables
                 6) A flag indicating if this is
                    a header call so that the
                    deffunction can be recursively
                    called
  RETURNS      : The new deffunction (NULL on errors)
  SIDE EFFECTS : Deffunction structures allocated
  NOTES        : Assumes deffunction is not executing
 ****************************************************/
static Deffunction *AddDeffunction(
  Environment *theEnv,
  SYMBOL_HN *name,
  EXPRESSION *actions,
  int min,
  int max,
  int lvars,
  bool headerp)
  {
   Deffunction *dfuncPtr;
   unsigned oldbusy;
#if DEBUGGING_FUNCTIONS
   bool DFHadWatch = false;
#else
#if MAC_XCD
#pragma unused(headerp)
#endif
#endif

   /*===============================================================*/
   /* If the deffunction doesn't exist, create a new structure to   */
   /* contain it and add it to the List of deffunctions. Otherwise, */
   /* use the existing structure and remove the pretty print form   */
   /* and interpretive code.                                        */
   /*===============================================================*/

   dfuncPtr = EnvFindDeffunctionInModule(theEnv,ValueToString(name));
   if (dfuncPtr == NULL)
     {
      dfuncPtr = get_struct(theEnv,deffunction);
      InitializeConstructHeader(theEnv,"deffunction",(struct constructHeader *) dfuncPtr,name);
      IncrementSymbolCount(name);
      dfuncPtr->code = NULL;
      dfuncPtr->minNumberOfParameters = min;
      dfuncPtr->maxNumberOfParameters = max;
      dfuncPtr->numberOfLocalVars = lvars;
      dfuncPtr->busy = 0;
      dfuncPtr->executing = 0;
     }
   else
     {
#if DEBUGGING_FUNCTIONS
      DFHadWatch = EnvGetDeffunctionWatch(theEnv,dfuncPtr);
#endif
      dfuncPtr->minNumberOfParameters = min;
      dfuncPtr->maxNumberOfParameters = max;
      dfuncPtr->numberOfLocalVars = lvars;
      oldbusy = dfuncPtr->busy;
      ExpressionDeinstall(theEnv,dfuncPtr->code);
      dfuncPtr->busy = oldbusy;
      ReturnPackedExpression(theEnv,dfuncPtr->code);
      dfuncPtr->code = NULL;
      EnvSetDeffunctionPPForm(theEnv,dfuncPtr,NULL);

      /*======================================*/
      /* Remove the deffunction from the list */
      /* so that it can be added at the end.  */
      /*======================================*/
      
      RemoveConstructFromModule(theEnv,(struct constructHeader *) dfuncPtr);
     }

   AddConstructToModule((struct constructHeader *) dfuncPtr);

   /*====================================*/
   /* Install the new interpretive code. */
   /*====================================*/

   if (actions != NULL)
     {
      /*=================================================*/
      /* If a deffunction is recursive, do not increment */
      /* its busy count based on self-references.        */
      /*=================================================*/
      
      oldbusy = dfuncPtr->busy;
      ExpressionInstall(theEnv,actions);
      dfuncPtr->busy = oldbusy;
      dfuncPtr->code = actions;
     }

   /*==================================*/
   /* Install the pretty print form if */
   /* memory is not being conserved.   */
   /*==================================*/

#if DEBUGGING_FUNCTIONS
   EnvSetDeffunctionWatch(theEnv,
                          DFHadWatch ? true : DeffunctionData(theEnv)->WatchDeffunctions,
                          dfuncPtr);
      
   if ((EnvGetConserveMemory(theEnv) == false) && (headerp == false))
     { EnvSetDeffunctionPPForm(theEnv,dfuncPtr,CopyPPBuffer(theEnv)); }
#endif

   return(dfuncPtr);
  }

#endif /* DEFFUNCTION_CONSTRUCT && (! BLOAD_ONLY) && (! RUN_TIME) */

