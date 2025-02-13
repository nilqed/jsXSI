   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*         DEFGLOBAL BASIC COMMANDS HEADER FILE        */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Correction for FalseSymbol/TrueSymbol. DR0859  */
/*                                                           */
/*            Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */
/*            Changed name of variable log to logName        */
/*            because of Unix compiler warnings of shadowed  */
/*            definitions.                                   */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Removed conditional code for unsupported       */
/*            compilers/operating systems (IBM_MCW,          */
/*            MAC_MCW, and IBM_TBC).                         */
/*                                                           */
/*            Added const qualifiers to remove C++           */
/*            deprecation warnings.                          */
/*                                                           */
/*            Moved WatchGlobals global to defglobalData.    */
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

#ifndef _H_globlbsc

#pragma once

#define _H_globlbsc

#include "evaluatn.h"
#include "globldef.h"

   void                           DefglobalBasicCommands(Environment *);
   void                           UndefglobalCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvUndefglobal(Environment *,Defglobal *);
   void                           GetDefglobalListFunction(Environment *,UDFContext *,CLIPSValue *);
   void                           EnvGetDefglobalList(Environment *,CLIPSValue *,Defmodule *);
   void                           DefglobalModuleFunction(Environment *,UDFContext *,CLIPSValue *);
   void                           PPDefglobalCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           PPDefglobal(Environment *,const char *,const char *);
   void                           ListDefglobalsCommand(Environment *,UDFContext *,CLIPSValue *);
#if DEBUGGING_FUNCTIONS
   bool                           EnvGetDefglobalWatch(Environment *,Defglobal *);
   void                           EnvListDefglobals(Environment *,const char *,Defmodule *);
   void                           EnvSetDefglobalWatch(Environment *,bool,Defglobal *);
#endif
   void                           ResetDefglobals(Environment *);

#endif /* _H_globlbsc */


