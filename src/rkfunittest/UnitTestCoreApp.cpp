#include "UnitTestCoreApp.h"

char UnitTestCoreApp::_arg0[] = "programName";
char *UnitTestCoreApp::_argv[] = { &UnitTestCoreApp::_arg0[0], nullptr};
int UnitTestCoreApp::_argc = 1;

UnitTestCoreApp::UnitTestCoreApp()
  : ExceptionSafeCoreApp(_argc, _argv) {}
