#!/usr/bin/env python
#-------------------------------------------------------------------------------
#   This file is part of the Code_Saturne Solver.
#
#   Copyright (C) 2011  EDF
#
#   The Code_Saturne Preprocessor is free software; you can redistribute it
#   and/or modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version 2 of
#   the License, or (at your option) any later version.
#
#   The Code_Saturne Preprocessor is distributed in the hope that it will be
#   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
#   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public Licence
#   along with the Code_Saturne Preprocessor; if not, write to the
#   Free Software Foundation, Inc.,
#   51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#-------------------------------------------------------------------------------


"""
This module describes the script used to launch SALOME with the CFDSTUDY module.

This module defines the following functions:
- process_cmd_line
"""


#-------------------------------------------------------------------------------
# Library modules import
#-------------------------------------------------------------------------------

import os, sys

from optparse import OptionParser
import ConfigParser

import cs_exec_environment
from cs_config import config

#-------------------------------------------------------------------------------
# Processes the passed command line arguments
#-------------------------------------------------------------------------------


def process_cmd_line(argv, pkg):
    """
    Processes the passed command line arguments.
    """

    parser = OptionParser(usage="usage: %prog [options]")

    (options, args) = parser.parse_args(argv)

    return


#-------------------------------------------------------------------------------
# Launch SALOME platform with CFDSTUDY module
#-------------------------------------------------------------------------------

def main(argv, pkg):
    """
    Main function.
    """

    template = """\
CFDSTUDY_ROOT_DIR=%(prefix)s; PYTHONPATH=%(pythondir)s:$PYTHONPATH;
export CFDSTUDY_ROOT_DIR PYTHONPATH;
%(runsalome)s --modules=%(modules)s
"""

    cfg = config()

    if cfg.have_salome == "no":
        sys.stderr.write("SALOME is not available in this installation.\n")
        sys.exit(1)

    # Skipped modules (version 6.3.0): YACS,JOBMANAGER,HOMARD,OPENTURNS
    default_modules = "GEOM,SMESH,MED,CFDSTUDY,PARAVIS,VISU"

    cmd = template % {'prefix': pkg.prefix,
                      'pythondir': pkg.pythondir,
                      'runsalome': os.path.join(cfg.salome_kernel,
                                                'bin', 'salome', 'runSalome'),
                      'modules': default_modules}

    process_cmd_line(argv, pkg)

    retcode = cs_exec_environment.run_command(cmd,
                                              stdout=None,
                                              stderr=None)


if __name__ == "__main__":
    main(sys.argv[1:], None)


#-------------------------------------------------------------------------------
# End
#-------------------------------------------------------------------------------
