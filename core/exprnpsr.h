   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*            EXPRESSION PARSER HEADER FILE            */
   /*******************************************************/

/*************************************************************/
/* Purpose: Provides routines for parsing expressions.       */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Changed name of variable exp to theExp         */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Module specifier can be used within an         */
/*            expression to refer to a deffunction or        */
/*            defgeneric exported by the specified module,   */
/*            but not necessarily imported by the current    */
/*            module.                                        */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Converted API macros to function calls.        */
/*                                                           */
/*      6.40: Removed LOCALE definition.                     */
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
/*************************************************************/

#ifndef _H_exprnpsr

#pragma once

#define _H_exprnpsr

#if (! RUN_TIME)

typedef struct saved_contexts
  {
   bool rtn;
   bool brk;
   struct saved_contexts *nxt;
  } SAVED_CONTEXTS;

#endif

#include "extnfunc.h"
#include "scanner.h"

   struct expr                   *Function0Parse(Environment *,const char *);
   struct expr                   *Function1Parse(Environment *,const char *);
   struct expr                   *Function2Parse(Environment *,const char *,const char *);
   void                           PushRtnBrkContexts(Environment *);
   void                           PopRtnBrkContexts(Environment *);
   bool                           ReplaceSequenceExpansionOps(Environment *,struct expr *,struct expr *,
                                                                     void *,void *);
   struct expr                   *CollectArguments(Environment *,struct expr *,const char *);
   struct expr                   *ArgumentParse(Environment *,const char *,bool *);
   struct expr                   *ParseAtomOrExpression(Environment *,const char *,struct token *);
   EXPRESSION                    *ParseConstantArguments(Environment *,const char *,bool *);
   bool                           EnvSetSequenceOperatorRecognition(Environment *,bool);
   bool                           EnvGetSequenceOperatorRecognition(Environment *);
   struct expr                   *GroupActions(Environment *,const char *,struct token *,
                                                      bool,const char *,bool);
   struct expr                   *RemoveUnneededProgn(Environment *,struct expr *);
   void                           PopulateRestriction(Environment *,unsigned *,unsigned,const char *,int);

#if (! RUN_TIME)

   bool                           CheckExpressionAgainstRestrictions(Environment *,struct expr *,
                                                                     struct FunctionDefinition *,const char *);
   bool                           RestrictionExists(const char *,int);
   
#endif

#endif /* _H_exprnpsr */




