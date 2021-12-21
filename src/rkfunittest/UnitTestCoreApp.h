// Copyright (C) 2017-2018 RKF Engineering Solutions, LLC
#ifndef SRC_CPOTESTCOMMON_UNITTESTGUIAPP_H_
#define SRC_CPOTESTCOMMON_UNITTESTGUIAPP_H_

#include "ratcommon/ExceptionSafeCoreApp.h"

/** A stand-alone QCoreApplication with no command argument dependence.
 */
class UnitTestCoreApp : public ExceptionSafeCoreApp {
 public:
    /** Construct a new application.
     */
    UnitTestCoreApp();

 private:
    /// Necessary storage as QCoreApplication keeps a reference
    static int _argc; // fake argc
    static char _arg0[]; // fake program name
    static char *_argv[]; // fake argv
};

#endif /* SRC_CPOTESTCOMMON_UNITTESTGUIAPP_H_ */
