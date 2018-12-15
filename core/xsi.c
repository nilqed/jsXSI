#include <stdio.h>
#include "setup.h"
#include "clips.h"  

int ExecOnce(void *, char * );
void UserFunctions(void);
void EnvUserFunctions(void *);
                              
                              
int ExecOnce(void *theEnv, char *str){
  FlushCommandString(theEnv) ;
  SetCommandString(theEnv, str) ;
  return ExecuteIfCommandComplete(theEnv);
};                              
                                            
