   /*******************************************************/
   /*      "C" Language Integrated Production System      */
   /*                                                     */
   /*            CLIPS Version 6.40  08/11/16             */
   /*                                                     */
   /*                  ANALYSIS MODULE                    */
   /*******************************************************/

/*************************************************************/
/* Purpose: Analyzes LHS patterns to check for semantic      */
/*   errors and to determine variable comparisons and other  */
/*   tests which must be performed either in the pattern or  */
/*   join networks.                                          */
/*                                                           */
/* Principal Programmer(s):                                  */
/*      Gary D. Riley                                        */
/*                                                           */
/* Contributing Programmer(s):                               */
/*                                                           */
/* Revision History:                                         */
/*                                                           */
/*      6.24: Renamed BOOLEAN macro type to intBool.         */
/*                                                           */
/*      6.30: Join network rework and optimizations.         */
/*                                                           */
/*      6.40: Pragma once and other inclusion changes.       */
/*                                                           */
/*            Added support for booleans with <stdbool.h>.   */
/*                                                           */
/*            Removed use of void pointers for specific      */
/*            data structures.                               */
/*                                                           */
/*            Static constraint checking is always enabled.  */
/*                                                           */
/*************************************************************/

#include "setup.h"

#if (! RUN_TIME) && (! BLOAD_ONLY) && DEFRULE_CONSTRUCT

#include <stdio.h>

#include "constant.h"
#include "cstrnchk.h"
#include "cstrnutl.h"
#include "cstrnops.h"
#include "exprnpsr.h"
#include "generate.h"
#include "memalloc.h"
#include "modulutl.h"
#include "pattern.h"
#include "reorder.h"
#include "router.h"
#include "rulecstr.h"
#include "ruledef.h"
#include "rulepsr.h"
#include "symbol.h"
#include "watch.h"

#include "analysis.h"

/***************************************/
/* LOCAL INTERNAL FUNCTION DEFINITIONS */
/***************************************/

   static bool                    GetVariables(Environment *,struct lhsParseNode *,int,struct nandFrame *);
   static bool                    UnboundVariablesInPattern(Environment *,struct lhsParseNode *,int);
   static bool                    PropagateVariableToNodes(Environment *,
                                                           struct lhsParseNode *,
                                                           int,
                                                           struct symbolHashNode *,
                                                           struct lhsParseNode *,
                                                           int,bool,bool);
   static struct lhsParseNode    *CheckExpression(Environment *,
                                                  struct lhsParseNode *,
                                                  struct lhsParseNode *,
                                                  int,
                                                  struct symbolHashNode *,
                                                  int);
   static void                    VariableReferenceErrorMessage(Environment *,
                                                                struct symbolHashNode *,
                                                                struct lhsParseNode *,
                                                                int,
                                                                struct symbolHashNode *,
                                                                int);
   static bool                    ProcessField(Environment *,
                                               struct lhsParseNode *,
                                               struct lhsParseNode *,
                                               struct lhsParseNode *,
                                               int,
                                               struct nandFrame *);
   static bool                    ProcessVariable(Environment *,
                                               struct lhsParseNode *,
                                               struct lhsParseNode *,
                                               struct lhsParseNode *,
                                               int,
                                               struct nandFrame *);
   static void                    VariableMixingErrorMessage(Environment *,struct symbolHashNode *);
   static bool                    PropagateVariableDriver(Environment *,
                                                          struct lhsParseNode *,
                                                          struct lhsParseNode *,
                                                          struct lhsParseNode *,
                                                          int,struct symbolHashNode *,
                                                          struct lhsParseNode *,
                                                          bool,int);
   static bool                    TestCEAnalysis(Environment *,struct lhsParseNode *,struct lhsParseNode *,bool,bool *,struct nandFrame *);
   static void                    ReleaseNandFrames(Environment *,struct nandFrame *);

/******************************************************************/
/* VariableAnalysis: Propagates variables references to other     */
/*   variables in the LHS and determines if there are any illegal */
/*   variable references (e.g. referring to an unbound variable). */
/*   The propagation of variable references simply means all      */
/*   subsequent references of a variable are made to "point" back */
/*   to the variable being propagated.                            */
/******************************************************************/
bool VariableAnalysis(
  Environment *theEnv,
  struct lhsParseNode *patternPtr)
  {
   bool errorFlag = false;
   struct nandFrame *theNandFrames = NULL, *tempNandPtr;
   int currentDepth = 1;

   /*======================================================*/
   /* Loop through all of the CEs in the rule to determine */
   /* which variables refer to other variables and whether */
   /* any semantic errors exist when refering to variables */
   /* (such as referring to a variable that was not        */
   /* previously bound).                                   */
   /*======================================================*/

   while (patternPtr != NULL)
     {
      /*==================================*/
      /* If the nand depth is increasing, */
      /* create a new nand frame.         */
      /*==================================*/

      while (patternPtr->beginNandDepth > currentDepth)
        {
         tempNandPtr = get_struct(theEnv,nandFrame);
         tempNandPtr->nandCE = patternPtr;
         tempNandPtr->depth = currentDepth;
         tempNandPtr->next = theNandFrames;
         theNandFrames = tempNandPtr;
         currentDepth++;
        }

      /*=========================================================*/
      /* If a pattern CE is encountered, propagate any variables */
      /* found in the pattern and note any illegal references to */
      /* other variables.                                        */
      /*=========================================================*/

      if (patternPtr->type == PATTERN_CE)
        {
         /*====================================================*/
         /* Determine if the fact address associated with this */
         /* pattern illegally refers to other variables.       */
         /*====================================================*/

         if ((patternPtr->value != NULL) &&
             (patternPtr->referringNode != NULL))
           {
            errorFlag = true;
            if (patternPtr->referringNode->index == -1)
              {
               PrintErrorID(theEnv,"ANALYSIS",1,true);
               EnvPrintRouter(theEnv,WERROR,"Duplicate pattern-address ?");
               EnvPrintRouter(theEnv,WERROR,ValueToString(patternPtr->value));
               EnvPrintRouter(theEnv,WERROR," found in CE #");
               PrintLongInteger(theEnv,WERROR,(long) patternPtr->whichCE);
               EnvPrintRouter(theEnv,WERROR,".\n");
              }
            else
              {
               PrintErrorID(theEnv,"ANALYSIS",2,true);
               EnvPrintRouter(theEnv,WERROR,"Pattern-address ?");
               EnvPrintRouter(theEnv,WERROR,ValueToString(patternPtr->value));
               EnvPrintRouter(theEnv,WERROR," used in CE #");
               PrintLongInteger(theEnv,WERROR,(long) patternPtr->whichCE);
               EnvPrintRouter(theEnv,WERROR," was previously bound within a pattern CE.\n");
              }
           }

         /*====================================================*/
         /* Propagate the pattern and field location of bound  */
         /* variables found in this pattern to other variables */
         /* in the same semantic scope as the bound variable.  */
         /*====================================================*/

         if (GetVariables(theEnv,patternPtr,PATTERN_CE,theNandFrames))
           {
            ReleaseNandFrames(theEnv,theNandFrames);
            return true;
           }
 
         /*==========================================================*/
         /* Analyze any test CE that's been attached to the pattern. */
         /*==========================================================*/
         
         if (TestCEAnalysis(theEnv,patternPtr,patternPtr->expression,false,&errorFlag,theNandFrames) == true)
           {
            ReleaseNandFrames(theEnv,theNandFrames);
            return true;
           }
 
         if (TestCEAnalysis(theEnv,patternPtr,patternPtr->secondaryExpression,true,&errorFlag,theNandFrames) == true)
           {
            ReleaseNandFrames(theEnv,theNandFrames);
            return true;
           }
        }

      /*==============================================================*/
      /* If a test CE is encountered, make sure that all references   */
      /* to variables have been previously bound. If they are bound   */
      /* then replace the references to variables with function calls */
      /* to retrieve the variables.                                   */
      /*==============================================================*/

      else if (patternPtr->type == TEST_CE)
        {
         if (TestCEAnalysis(theEnv,patternPtr,patternPtr->expression,false,&errorFlag,theNandFrames) == true)
           {
            ReleaseNandFrames(theEnv,theNandFrames);
            return true;
           }
        }

      /*==================================*/
      /* If the nand depth is decreasing, */
      /* release the nand frames.         */
      /*==================================*/

      while (patternPtr->endNandDepth < currentDepth)
        {
         tempNandPtr = theNandFrames->next;
         rtn_struct(theEnv,nandFrame,theNandFrames);
         theNandFrames = tempNandPtr;
         currentDepth--;
        }

      /*=====================================================*/
      /* Move on to the next pattern in the LHS of the rule. */
      /*=====================================================*/

      patternPtr = patternPtr->bottom;
     }

   /*==========================================*/
   /* Return the error status of the analysis. */
   /*==========================================*/

   return(errorFlag);
  }

/******************************************************/
/* ReleaseNandFrames: Releases a list of nand frames. */
/******************************************************/
static void ReleaseNandFrames(
  Environment *theEnv,
  struct nandFrame *theFrames)
  {
   struct nandFrame *tmpFrame;
   
   while (theFrames != NULL)
     {
      tmpFrame = theFrames->next;
      rtn_struct(theEnv,nandFrame,theFrames);
      theFrames = tmpFrame;
     }
  }

/*******************************************************************/
/* TestCEAnalysis: If a test CE is encountered, make sure that all */
/*   references to variables have been previously bound. If they   */
/*   are bound then replace the references to variables with       */
/*   function calls to retrieve the variables.                     */
/*******************************************************************/
static bool TestCEAnalysis(
  Environment *theEnv,
  struct lhsParseNode *patternPtr,
  struct lhsParseNode *theExpression,
  bool secondary,
  bool *errorFlag,
  struct nandFrame *theNandFrames)
  {
   struct lhsParseNode *rv, *theList, *tempList, *tempRight;

   if (theExpression == NULL) return false;
   
   /*=====================================================*/
   /* Verify that all variables were referenced properly. */
   /*=====================================================*/

   rv = CheckExpression(theEnv,theExpression,NULL,(int) patternPtr->whichCE,NULL,0);

   /*====================================================================*/
   /* Temporarily disconnect the right nodes. If this is a pattern node  */
   /* with an attached test CE, we only want to propagate to following   */
   /* patterns, not to nodes of this pattern which preceded the test CE. */
   /*====================================================================*/
   
   tempRight = patternPtr->right;
   patternPtr->right = NULL;
      
   /*=========================================================*/
   /* Determine the type and value constraints implied by the */
   /* expression and propagate these constraints to other     */
   /* variables in the LHS. For example, the expression       */
   /* (+ ?x 1) implies that ?x is a number.                   */
   /*=========================================================*/
   
   theList = GetExpressionVarConstraints(theEnv,theExpression);
   for (tempList = theList; tempList != NULL; tempList = tempList->right)
      {
       if (PropagateVariableDriver(theEnv,patternPtr,patternPtr,NULL,SF_VARIABLE,
                                   (SYMBOL_HN *) tempList->value,tempList,false,TEST_CE))
         {
          ReturnLHSParseNodes(theEnv,theList);
          patternPtr->right = tempRight;
          return true;
         }
      }
      
   ReturnLHSParseNodes(theEnv,theList);
   
   /*============================*/
   /* Reconnect the right nodes. */
   /*============================*/
   
   patternPtr->right = tempRight;
   
   /*========================================================*/
   /* If the variables in the expression were all referenced */
   /* properly, then create the expression to use in the     */
   /* join network.                                          */
   /*========================================================*/

   if (rv != NULL)
     { *errorFlag = true; }
   else if (secondary)
     { patternPtr->secondaryNetworkTest = CombineExpressions(theEnv,patternPtr->secondaryNetworkTest,GetvarReplace(theEnv,theExpression,false,theNandFrames)); }
   else
     { patternPtr->networkTest = CombineExpressions(theEnv,patternPtr->networkTest,GetvarReplace(theEnv,theExpression,false,theNandFrames)); }
     
   return false;
  }

/****************************************************************/
/* GetVariables: Loops through each field/slot within a pattern */
/*   and propagates the pattern and field location of bound     */
/*   variables found in the pattern to other variables within   */
/*   the same semantic scope as the bound variables.            */
/****************************************************************/
static bool GetVariables(
  Environment *theEnv,
  struct lhsParseNode *thePattern,
  int patternHeadType,
  struct nandFrame *theNandFrames)
  {
   struct lhsParseNode *patternHead = thePattern;
   struct lhsParseNode *multifieldHeader = NULL;

   /*======================================================*/
   /* Loop through all the fields/slots found in a pattern */
   /* looking for binding instances of variables.          */
   /*======================================================*/

   while (thePattern != NULL)
     {
      /*================================================*/
      /* A multifield slot contains a sublist of fields */
      /* that must be traversed and checked.            */
      /*================================================*/

      if (thePattern->multifieldSlot)
        {
         multifieldHeader = thePattern;
         thePattern = thePattern->bottom;
        }

      /*==================================================*/
      /* Propagate the binding occurences of single field */
      /* variables, multifield variables, and fact        */
      /* addresses to other occurences of the variable.   */
      /* If an error is encountered, return true.         */
      /*==================================================*/

      if (thePattern != NULL)
        {
         if ((thePattern->type == SF_VARIABLE) ||
             (thePattern->type == MF_VARIABLE) ||
             ((thePattern->type == PATTERN_CE) && (thePattern->value != NULL)))
           {
            if (ProcessVariable(theEnv,thePattern,multifieldHeader,patternHead,patternHeadType,theNandFrames))
              { return true; }
           }
         else
           {
            if (ProcessField(theEnv,thePattern,multifieldHeader,patternHead,patternHeadType,theNandFrames))
              { return true; }
           }
        }

      /*===============================================*/
      /* Move on to the next field/slot in the pattern */
      /* or to the next field in a multifield slot.    */
      /*===============================================*/

      if (thePattern == NULL)
        { thePattern = multifieldHeader; }
      else if ((thePattern->right == NULL) && (multifieldHeader != NULL))
        {
         thePattern = multifieldHeader;
         multifieldHeader = NULL;
        }

      thePattern = thePattern->right;
     }

   /*===============================*/
   /* Return false to indicate that */
   /* no errors were detected.      */
   /*===============================*/

   return false;
  }

/******************************************************/
/* ProcessVariable: Processes a single occurence of a */
/*   variable by propagating references to it.        */
/******************************************************/
static bool ProcessVariable(
  Environment *theEnv,
  struct lhsParseNode *thePattern,
  struct lhsParseNode *multifieldHeader,
  struct lhsParseNode *patternHead,
  int patternHeadType,
  struct nandFrame *theNandFrames)
  {
   int theType;
   struct symbolHashNode *theVariable;
   struct constraintRecord *theConstraints;

   /*=============================================================*/
   /* If a pattern address is being propagated, then treat it as  */
   /* a single field pattern variable and create a constraint     */
   /* which indicates that is must be a fact or instance address. */
   /* This code will have to be modified for new data types which */
   /* can match patterns.                                         */
   /*=============================================================*/

   if (thePattern->type == PATTERN_CE)
     {
      theType = SF_VARIABLE;
      theVariable = (struct symbolHashNode *) thePattern->value;
      if (thePattern->derivedConstraints) RemoveConstraint(theEnv,thePattern->constraints);
      theConstraints = GetConstraintRecord(theEnv);
      thePattern->constraints = theConstraints;
      thePattern->constraints->anyAllowed = false;
      thePattern->constraints->instanceAddressesAllowed = true;
      thePattern->constraints->factAddressesAllowed = true;
      thePattern->derivedConstraints = true;
     }

   /*===================================================*/
   /* Otherwise a pattern variable is being propagated. */
   /*===================================================*/

   else
     {
      theType = thePattern->type;
      theVariable = (struct symbolHashNode *) thePattern->value;
     }

   /*===================================================*/
   /* Propagate the variable location to any additional */
   /* fields associated with the binding variable.      */
   /*===================================================*/

   if (thePattern->type != PATTERN_CE)
     {
      PropagateVariableToNodes(theEnv,thePattern->bottom,theType,theVariable,
                               thePattern,patternHead->beginNandDepth,
                               true,false);

      if (ProcessField(theEnv,thePattern,multifieldHeader,patternHead,patternHeadType,theNandFrames))
        { return true; }
     }

   /*=================================================================*/
   /* Propagate the constraints to other fields, slots, and patterns. */
   /*=================================================================*/

   return(PropagateVariableDriver(theEnv,patternHead,thePattern,multifieldHeader,theType,
                                  theVariable,thePattern,true,patternHeadType));
  }

/*******************************************/
/* PropagateVariableDriver: Driver routine */
/*   for propagating variable references.  */
/*******************************************/
static bool PropagateVariableDriver(
  Environment *theEnv,
  struct lhsParseNode *patternHead,
  struct lhsParseNode *theNode,
  struct lhsParseNode *multifieldHeader,
  int theType,
  struct symbolHashNode *variableName,
  struct lhsParseNode *theReference,
  bool assignReference,
  int patternHeadType)
  {
   /*===================================================*/
   /* Propagate the variable location to any additional */
   /* constraints associated with the binding variable. */
   /*===================================================*/

   if (multifieldHeader != NULL)
     {
      if (PropagateVariableToNodes(theEnv,multifieldHeader->right,theType,variableName,
                                   theReference,patternHead->beginNandDepth,assignReference,false))
        {
         VariableMixingErrorMessage(theEnv,variableName);
         return true;
        }
     }

   /*========================================================*/
   /* Propagate the variable location to fields/slots in the */
   /* same pattern which appear after the binding variable.  */
   /*========================================================*/

   if (PropagateVariableToNodes(theEnv,theNode->right,theType,variableName,theReference,
                                patternHead->beginNandDepth,assignReference,false))
     {
      VariableMixingErrorMessage(theEnv,variableName);
      return true;
     }

   /*==============================================*/
   /* Propagate the variable location to any test  */
   /* CEs which have been attached to the pattern. */
   /*==============================================*/

   if (PropagateVariableToNodes(theEnv,patternHead->expression,theType,variableName,theReference,
                                patternHead->beginNandDepth,assignReference,true))
     { return true; }

   if (PropagateVariableToNodes(theEnv,patternHead->secondaryExpression,theType,variableName,theReference,
                                patternHead->beginNandDepth,assignReference,true))
     { return true; }
   
   /*======================================================*/
   /* Propagate values to other patterns if the pattern in */
   /* which the variable is found is not a "not" CE or the */
   /* last pattern within a nand CE.                       */
   /*======================================================*/

   if (((patternHead->type == PATTERN_CE) || (patternHead->type == TEST_CE)) &&
       (patternHead->negated == false) &&
       (patternHead->exists == false) &&
       (patternHead->beginNandDepth <= patternHead->endNandDepth))
     {
      bool ignoreVariableMixing;

      /*============================================================*/
      /* If the variables are propagated from a test CE, then don't */
      /* check for mixing of single and multifield variables (since */
      /* previously bound multifield variables typically have the $ */
      /* removed when passed as an argument to a function unless    */
      /* sequence expansion is desired).                            */
      /*============================================================*/

      if (patternHeadType == TEST_CE) ignoreVariableMixing = true;
      else ignoreVariableMixing = false;

      /*==========================*/
      /* Propagate the reference. */
      /*==========================*/

      if (PropagateVariableToNodes(theEnv,patternHead->bottom,theType,variableName,theReference,
                                   patternHead->beginNandDepth,assignReference,
                                   ignoreVariableMixing))
       {
         VariableMixingErrorMessage(theEnv,variableName);
         return true;
        }
     }

   /*==============================================*/
   /* Return false to indicate that no errors were */
   /* generated by the variable propagation.       */
   /*==============================================*/

   return false;
  }

/********************************************************/
/* ProcessField: Processes a field or slot of a pattern */
/*   which does not contain a binding variable.         */
/********************************************************/
static bool ProcessField(
  Environment *theEnv,
  struct lhsParseNode *thePattern,
  struct lhsParseNode *multifieldHeader,
  struct lhsParseNode *patternHead,
  int patternHeadType,
  struct nandFrame *theNandFrames)
  {
   struct lhsParseNode *theList, *tempList;

   /*====================================================*/
   /* Nothing needs to be done for the node representing */
   /* the entire pattern. Return false to indicate that  */
   /* no errors were generated.                          */
   /*====================================================*/

   if (thePattern->type == PATTERN_CE) return false;

   /*====================================================================*/
   /* Derive a set of constraints based on values found in the slot or   */
   /* field. For example, if a slot can only contain the values 1, 2, or */
   /* 3, the field constraint ~2 would generate a constraint record that */
   /* only allows the value 1 or 3. Once generated, the constraints are  */
   /* propagated to other slots and fields.                              */
   /*====================================================================*/

   theList = DeriveVariableConstraints(theEnv,thePattern);
   for (tempList = theList; tempList != NULL; tempList = tempList->right)
     {
      if (PropagateVariableDriver(theEnv,patternHead,thePattern,multifieldHeader,tempList->type,
                                  (SYMBOL_HN *) tempList->value,tempList,false,patternHeadType))
        {
         ReturnLHSParseNodes(theEnv,theList);
         return true;
        }
     }
   ReturnLHSParseNodes(theEnv,theList);

   /*===========================================================*/
   /* Check for "variable referenced, but not previously bound" */
   /* errors. Return true if this type of error is detected.    */
   /*===========================================================*/

   if (UnboundVariablesInPattern(theEnv,thePattern,(int) patternHead->whichCE))
     { return true; }

   /*==================================================*/
   /* Check for constraint errors for this slot/field. */
   /* If the slot/field has unmatchable constraints    */
   /* then return true to indicate a semantic error.   */
   /*==================================================*/

   if (ProcessConnectedConstraints(theEnv,thePattern,multifieldHeader,patternHead))
     { return true; }

   /*==============================================================*/
   /* Convert the slot/field constraint to a series of expressions */
   /* that will be used in the pattern and join networks.          */
   /*==============================================================*/

   FieldConversion(theEnv,thePattern,patternHead,theNandFrames);

   /*=========================================================*/
   /* Return false to indicate that no errors were generated. */
   /*=========================================================*/

   return false;
  }

/*************************************************************/
/* PropagateVariableToNodes: Propagates variable references  */
/*  to all other variables within the semantic scope of the  */
/*  bound variable. That is, a variable reference cannot be  */
/*  beyond an enclosing not/and CE combination. The          */
/*  restriction of propagating variables beyond an enclosing */
/*  not CE is handled within the GetVariables function.      */
/*************************************************************/
static bool PropagateVariableToNodes(
  Environment *theEnv,
  struct lhsParseNode *theNode,
  int theType,
  struct symbolHashNode *variableName,
  struct lhsParseNode *theReference,
  int startDepth,
  bool assignReference,
  bool ignoreVariableTypes)
  {
   struct constraintRecord *tempConstraints;

   /*===========================================*/
   /* Traverse the nodes using the bottom link. */
   /*===========================================*/

   while (theNode != NULL)
     {
      /*==================================================*/
      /* If the field/slot contains a predicate or return */
      /* value constraint, then propagate the variable to */
      /* the expression associated with that constraint.  */
      /*==================================================*/

      if (theNode->expression != NULL)
        {
         PropagateVariableToNodes(theEnv,theNode->expression,theType,variableName,
                                  theReference,startDepth,assignReference,true);
        }

      if (theNode->secondaryExpression != NULL)
        {
         PropagateVariableToNodes(theEnv,theNode->secondaryExpression,theType,variableName,
                                  theReference,startDepth,assignReference,true);
        }
        
      /*======================================================*/
      /* If the field/slot is a single or multifield variable */
      /* with the same name as the propagated variable,       */
      /* then propagate the variable location to this node.   */
      /*======================================================*/

      else if (((theNode->type == SF_VARIABLE) || (theNode->type == MF_VARIABLE)) &&
               (theNode->value == (void *) variableName))
        {
         /*======================================================*/
         /* Check for mixing of single and multifield variables. */
         /*======================================================*/

         if (ignoreVariableTypes == false)
           {
            if (((theType == SF_VARIABLE) && (theNode->type == MF_VARIABLE)) ||
                ((theType == MF_VARIABLE) && (theNode->type == SF_VARIABLE)))
              { return true; }
           }

         /*======================================================*/
         /* Intersect the propagated variable's constraints with */
         /* the current constraints for this field/slot.         */
         /*======================================================*/

         if ((theReference->constraints != NULL) && (! theNode->negated))
           {
            tempConstraints = theNode->constraints;
            theNode->constraints = IntersectConstraints(theEnv,theReference->constraints,
                                                        tempConstraints);
            if (theNode->derivedConstraints)
              { RemoveConstraint(theEnv,tempConstraints); }

            theNode->derivedConstraints = true;
           }

         /*=====================================================*/
         /* Don't propagate the variable if it originates from  */
         /* a different type of pattern object and the variable */
         /* reference has already been resolved.                */
         /*=====================================================*/

         if (assignReference)
           {
            if (theNode->referringNode == NULL)
              { theNode->referringNode = theReference; }
            else if (theReference->pattern == theNode->pattern)
              { theNode->referringNode = theReference; }
            else if (theReference->patternType == theNode->patternType)
              { theNode->referringNode = theReference; }
           }
        }

      /*========================================================*/
      /* If the field/slot is the node representing the entire  */
      /* pattern, then propagate the variable location to the   */
      /* fact address associated with the pattern (if it is the */
      /* same variable name).                                   */
      /*========================================================*/

      else if ((theNode->type == PATTERN_CE) &&
               (theNode->value == (void *) variableName) &&
               (assignReference == true))
        {
         if (theType == MF_VARIABLE) return true;

         theNode->referringNode = theReference;
        }

      /*=====================================================*/
      /* Propagate the variable to other fields contained    */
      /* within the same & field constraint or same pattern. */
      /*=====================================================*/

      if (theNode->right != NULL)
        {
         if (PropagateVariableToNodes(theEnv,theNode->right,theType,variableName,
                                      theReference,startDepth,assignReference,ignoreVariableTypes))
           { return true; }
        }

      /*============================================================*/
      /* Propagate the variable to other patterns within the same   */
      /* semantic scope (if dealing with the node for an entire     */
      /* pattern) or to the next | field constraint within a field. */
      /*============================================================*/

      if ((theNode->type == PATTERN_CE) || (theNode->type == TEST_CE))
        {
         if (theNode->endNandDepth < startDepth) theNode = NULL;
         else theNode = theNode->bottom;
        }
      else
        { theNode = theNode->bottom; }
     }

   /*========================================================*/
   /* Return false to indicate that no errors were detected. */
   /*========================================================*/

   return false;
  }

/*************************************************************/
/* UnboundVariablesInPattern: Verifies that variables within */
/*   a slot/field have been referenced properly (i.e. that   */
/*   variables have been previously bound if they are not a  */
/*   binding occurrence).                                    */
/*************************************************************/
static bool UnboundVariablesInPattern(
  Environment *theEnv,
  struct lhsParseNode *theSlot,
  int pattern)
  {
   struct lhsParseNode *andField;
   struct lhsParseNode *rv;
   int result;
   struct lhsParseNode *orField;
   struct symbolHashNode *slotName;
   CONSTRAINT_RECORD *theConstraints;
   int theField;

   /*===================================================*/
   /* If a multifield slot is being checked, then check */
   /* each of the fields grouped with the multifield.   */
   /*===================================================*/

   if (theSlot->multifieldSlot)
     {
      theSlot = theSlot->bottom;
      while (theSlot != NULL)
        {
         if (UnboundVariablesInPattern(theEnv,theSlot,pattern))
           { return true; }
         theSlot = theSlot->right;
        }

      return false;
     }

   /*=======================*/
   /* Check a single field. */
   /*=======================*/

   slotName = theSlot->slot;
   theField = theSlot->index;
   theConstraints = theSlot->constraints;

   /*===========================================*/
   /* Loop through each of the '|' constraints. */
   /*===========================================*/

   for (orField = theSlot->bottom;
        orField != NULL;
        orField = orField->bottom)
     {
      /*===========================================*/
      /* Loop through each of the fields connected */
      /* by the '&' within the '|' constraint.     */
      /*===========================================*/

      for (andField = orField;
           andField != NULL;
           andField = andField->right)
        {
         /*=======================================================*/
         /* If this is not a binding occurence of a variable and  */
         /* there is no previous binding occurence of a variable, */
         /* then generate an error message for a variable that is */
         /* referred to but not bound.                            */
         /*=======================================================*/

         if (((andField->type == SF_VARIABLE) || (andField->type == MF_VARIABLE)) &&
             (andField->referringNode == NULL))
           {
            VariableReferenceErrorMessage(theEnv,(SYMBOL_HN *) andField->value,NULL,pattern,
                                          slotName,theField);
            return true;
           }

         /*==============================================*/
         /* Check predicate and return value constraints */
         /* to insure that all variables used within the */
         /* constraint have been previously bound.       */
         /*==============================================*/

         else if ((andField->type == PREDICATE_CONSTRAINT) ||
                  (andField->type == RETURN_VALUE_CONSTRAINT))
           {
            rv = CheckExpression(theEnv,andField->expression,NULL,pattern,slotName,theField);
            if (rv != NULL) return true;
           }

         /*========================================================*/
         /* If static constraint checking is being performed, then */
         /* determine if constant values have violated the set of  */
         /* derived constraints for the slot/field (based on the   */
         /* deftemplate definition and propagated constraints).    */
         /*========================================================*/

         else if (((andField->type == INTEGER) || (andField->type == FLOAT) ||
                   (andField->type == SYMBOL) || (andField->type == STRING) ||
                   (andField->type == INSTANCE_NAME)))
           {
            result = ConstraintCheckValue(theEnv,andField->type,andField->value,theConstraints);
            if (result != NO_VIOLATION)
              {
               ConstraintViolationErrorMessage(theEnv,"A literal restriction value",
                                               NULL,false,pattern,
                                               slotName,theField,result,
                                               theConstraints,true);
               return true;
              }
           }
        }
     }

   /*===============================*/
   /* Return false to indicate that */
   /* no errors were detected.      */
   /*===============================*/

   return false;
  }

/******************************************************************/
/* CheckExpression: Verifies that variables within an expression  */
/*   have been referenced properly. All variables within an       */
/*   expression must have been previously bound.                  */
/******************************************************************/
static struct lhsParseNode *CheckExpression(
  Environment *theEnv,
  struct lhsParseNode *exprPtr,
  struct lhsParseNode *lastOne,
  int whichCE,
  struct symbolHashNode *slotName,
  int theField)
  {
   struct lhsParseNode *rv;
   int i = 1;

   while (exprPtr != NULL)
     {
      /*===============================================================*/
      /* Check that single field variables contained in the expression */
      /* were previously defined in the LHS. Also check to see if the  */
      /* variable has unmatchable constraints.                         */
      /*===============================================================*/

      if (exprPtr->type == SF_VARIABLE)
        {
         if (exprPtr->referringNode == NULL)
           {
            VariableReferenceErrorMessage(theEnv,(SYMBOL_HN *) exprPtr->value,lastOne,
                                          whichCE,slotName,theField);
            return(exprPtr);
           }
         else if (UnmatchableConstraint(exprPtr->constraints))
           {
            ConstraintReferenceErrorMessage(theEnv,(SYMBOL_HN *) exprPtr->value,lastOne,i,
                                            whichCE,slotName,theField);
            return(exprPtr);
           }
        }

      /*==================================================*/
      /* Check that multifield variables contained in the */
      /* expression were previously defined in the LHS.   */
      /*==================================================*/

      else if ((exprPtr->type == MF_VARIABLE) && (exprPtr->referringNode == NULL))
        {
         VariableReferenceErrorMessage(theEnv,(SYMBOL_HN *) exprPtr->value,lastOne,
                                       whichCE,slotName,theField);
         return(exprPtr);
        }

      /*=====================================================*/
      /* Check that global variables are referenced properly */
      /* (i.e. if you reference a global variable, it must   */
      /* already be defined by a defglobal construct).       */
      /*=====================================================*/

#if DEFGLOBAL_CONSTRUCT
      else if (exprPtr->type == GBL_VARIABLE)
        {
         int count;

         if (FindImportedConstruct(theEnv,"defglobal",NULL,ValueToString(exprPtr->value),
                                   &count,true,NULL) == NULL)
           {
            VariableReferenceErrorMessage(theEnv,(SYMBOL_HN *) exprPtr->value,lastOne,
                                          whichCE,slotName,theField);
            return(exprPtr);
           }
        }
#endif

      /*============================================*/
      /* Recursively check other function calls to  */
      /* insure variables are referenced correctly. */
      /*============================================*/

      else if (((exprPtr->type == FCALL)
#if DEFGENERIC_CONSTRUCT
             || (exprPtr->type == GCALL)
#endif
#if DEFFUNCTION_CONSTRUCT
             || (exprPtr->type == PCALL)
#endif
         ) && (exprPtr->bottom != NULL))
        {
         if ((rv = CheckExpression(theEnv,exprPtr->bottom,exprPtr,whichCE,slotName,theField)) != NULL)
           { return(rv); }
        }

      /*=============================================*/
      /* Move on to the next part of the expression. */
      /*=============================================*/

      i++;
      exprPtr = exprPtr->right;
     }

   /*================================================*/
   /* Return NULL to indicate no error was detected. */
   /*================================================*/

   return NULL;
  }

/********************************************************/
/* VariableReferenceErrorMessage: Generic error message */
/*   for referencing a variable before it is defined.   */
/********************************************************/
static void VariableReferenceErrorMessage(
  Environment *theEnv,
  struct symbolHashNode *theVariable,
  struct lhsParseNode *theExpression,
  int whichCE,
  struct symbolHashNode *slotName,
  int theField)
  {
   struct expr *temprv;

   /*=============================*/
   /* Print the error message ID. */
   /*=============================*/

   PrintErrorID(theEnv,"ANALYSIS",4,true);

   /*=================================*/
   /* Print the name of the variable. */
   /*=================================*/

   EnvPrintRouter(theEnv,WERROR,"Variable ?");
   EnvPrintRouter(theEnv,WERROR,ValueToString(theVariable));
   EnvPrintRouter(theEnv,WERROR," ");

   /*=================================================*/
   /* If the variable was found inside an expression, */
   /* then print the expression.                      */
   /*=================================================*/

   if (theExpression != NULL)
     {
      whichCE = theExpression->whichCE;
      temprv = LHSParseNodesToExpression(theEnv,theExpression);
      ReturnExpression(theEnv,temprv->nextArg);
      temprv->nextArg = NULL;
      EnvPrintRouter(theEnv,WERROR,"found in the expression ");
      PrintExpression(theEnv,WERROR,temprv);
      EnvPrintRouter(theEnv,WERROR,"\n");
      ReturnExpression(theEnv,temprv);
     }

   /*====================================================*/
   /* Print the CE in which the variable was referenced. */
   /*====================================================*/

   EnvPrintRouter(theEnv,WERROR,"was referenced in CE #");
   PrintLongInteger(theEnv,WERROR,(long int) whichCE);

   /*=====================================*/
   /* Identify the slot or field in which */
   /* the variable was found.             */
   /*=====================================*/

   if (slotName == NULL)
     {
      if (theField > 0)
        {
         EnvPrintRouter(theEnv,WERROR," field #");
         PrintLongInteger(theEnv,WERROR,(long int) theField);
        }
     }
   else
     {
      EnvPrintRouter(theEnv,WERROR," slot ");
      EnvPrintRouter(theEnv,WERROR,ValueToString(slotName));
     }

   EnvPrintRouter(theEnv,WERROR," before being defined.\n");
  }

/************************************************************/
/* VariableMixingErrorMessage: Prints the error message for */
/*   the illegal mixing of single and multifield variables  */
/*   on the LHS of a rule.                                  */
/************************************************************/
static void VariableMixingErrorMessage(
  Environment *theEnv,
  struct symbolHashNode *theVariable)
  {
   PrintErrorID(theEnv,"ANALYSIS",3,true);
   EnvPrintRouter(theEnv,WERROR,"Variable ?");
   EnvPrintRouter(theEnv,WERROR,ValueToString(theVariable));
   EnvPrintRouter(theEnv,WERROR," is used as both a single and multifield variable in the LHS\n");
  }
  
#endif /* (! RUN_TIME) && (! BLOAD_ONLY) && DEFRULE_CONSTRUCT */


