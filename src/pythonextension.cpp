#include "frepple.h"
#include "freppleinterface.h"
using namespace frepple;

/** Used to initialize frePPLe as a Python extension module. */
PyMODINIT_FUNC PyInit_frepple(void)
{
  try
  {
    // Initialize frePPLe, without reading the configuration
    // files init.xml or init.py
    FreppleInitialize(false);
    return PythonInterpreter::getModule();
  }
  catch(const exception& e)
  {
    PyErr_SetString(PyExc_SystemError, e.what());
    return nullptr;
  }
  catch (...)
  {
    PyErr_SetString(PyExc_SystemError, "Initialization failed");
    return nullptr;
  }
}
