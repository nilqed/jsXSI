   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*             CLIPS Version 6.40  08/25/16            */
   /*                                                     */
   /*             CLASS EXAMINATION HEADER FILE           */
   /*******************************************************/

/*************************************************************/
/* Purpose:                                                  */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Brian L. Dantes                                      */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.23: Corrected compilation errors for files         */
/*            generated by constructs-to-c. DR0861           */
/*                                                           */                                      
/*      6.24: The DescribeClass macros were incorrectly      */
/*            defined. DR0862                                */
/*                                                           */
/*            Added allowed-classes slot facet.              */
/*                                                           */
/*            Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Added EnvSlotDefaultP function.                */
/*                                                           */
/*            Borland C (IBM_TBC) and Metrowerks CodeWarrior */
/*            (MAC_MCW, IBM_MCW) are no longer supported.    */
/*                                                           */
/*            Used gensprintf and genstrcat instead of       */
/*            sprintf and strcat.                            */
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

#ifndef _H_classexm

#pragma once

#define _H_classexm

#if DEBUGGING_FUNCTIONS

   void                           BrowseClassesCommand(Environment *,UDFContext *,CLIPSValue *);
   void                           EnvBrowseClasses(Environment *,const char *,Defclass *);
   void                           DescribeClassCommand(Environment *,UDFContext *,CLIPSValue *);
   void                           EnvDescribeClass(Environment *,const char *,Defclass *);

#endif /* DEBUGGING_FUNCTIONS */

   const char                    *GetCreateAccessorString(SlotDescriptor *);
   void                           GetDefclassModuleCommand(Environment *,UDFContext *,CLIPSValue *);
   void                           SuperclassPCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSuperclassP(Environment *,Defclass *,Defclass *);
   void                           SubclassPCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSubclassP(Environment *,Defclass *,Defclass *);
   void                           SlotExistPCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotExistP(Environment *,Defclass *,const char *,bool);
   void                           MessageHandlerExistPCommand(Environment *,UDFContext *,CLIPSValue *);
   void                           SlotWritablePCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotWritableP(Environment *,Defclass *,const char *);
   void                           SlotInitablePCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotInitableP(Environment *,Defclass *,const char *);
   void                           SlotPublicPCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotPublicP(Environment *,Defclass *,const char *);
   void                           SlotDirectAccessPCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotDirectAccessP(Environment *,Defclass *,const char *);
   void                           SlotDefaultValueCommand(Environment *,UDFContext *,CLIPSValue *);
   bool                           EnvSlotDefaultValue(Environment *,Defclass *,const char *,CLIPSValue *);
   void                           ClassExistPCommand(Environment *,UDFContext *,CLIPSValue *);
   int                            EnvSlotDefaultP(Environment *,Defclass *,const char *);
  
#endif /* _H_classexm */